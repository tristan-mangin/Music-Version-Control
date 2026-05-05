// the orchestrator. This is the only file that knows about the whole system and calls the
// others. Each user command (init, add, commit, log, checkout) is a method here. It holds
// a path to the repo root and creates the ObjectStore, Index, etc. as member objects.

// Class: Repository
//      public:
//          init(), add(filePath), commit(message), log(), checkout(hashOrBranch, outputPath)
//      private:
//          resolveRef(name) -> hash, headHash() -> string, writeHead(hash), findRepoRoot() -> path
#include "repository.h"
#include "object_store.h"
#include "commit.h"
#include "index.h"
#include "hasher.h"
#include "utils.h"
#include <iostream>
#include <stdexcept>
#include <ctime>
#include <vector>
#include <fstream>
#include <regex>

// Helper functions for .bvcsignore parsing and matching.
namespace
{
    /**
     * Trims leading and trailing whitespace from a string.
     * @param s The string to trim.
     * @return A new string with leading and trailing whitespace removed.
     */
    std::string trim(const std::string &s)
    {
        const std::string whitespace = " \t\r\n";
        const size_t start = s.find_first_not_of(whitespace);
        if (start == std::string::npos)
        {
            return "";
        }
        const size_t end = s.find_last_not_of(whitespace);
        return s.substr(start, end - start + 1);
    }

    /**
     * Converts a glob pattern (with * and ?) to a regular expression string.
     * @param pattern The glob pattern to convert.
     * @return A string representing the equivalent regular expression.
     */
    std::string globToRegex(const std::string &pattern)
    {
        std::string regexPattern = "^";
        for (char c : pattern)
        {
            switch (c)
            {
            case '*':
                regexPattern += ".*";
                break;
            case '?':
                regexPattern += ".";
                break;
            case '.':
            case '+':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '^':
            case '$':
            case '|':
            case '\\':
                regexPattern += "\\";
                regexPattern += c;
                break;
            default:
                regexPattern += c;
                break;
            }
        }
        regexPattern += "$";
        return regexPattern;
    }

    /**
     * Matches a text string against a glob pattern. The pattern can include * and ? wildcards.
     * @param pattern The glob pattern to match against (e.g., "*.tmp").
     * @param text The text string to test (e.g., "scratch.tmp").
     * @return True if the text matches the pattern, false otherwise.
     */
    bool wildcardMatch(const std::string &pattern, const std::string &text)
    {
        return std::regex_match(text, std::regex(globToRegex(pattern)));
    }

    /**
     * Determines if the target path is relative to the base path and returns the relative path if so.
     * @param base The base path to compare against.
     * @param target The target path to test.
     * @param relativeOut Output parameter that will contain the relative path if the target is relative to the base.
     * @return True if the target is relative to the base, false otherwise
     */
    bool isRelativeTo(const std::filesystem::path &base, const std::filesystem::path &target, std::string &relativeOut)
    {
        std::error_code ec;
        std::filesystem::path rel = std::filesystem::relative(target, base, ec);
        if (ec)
        {
            return false;
        }

        std::string relStr = rel.generic_string();
        if (relStr.empty() || relStr == ".")
        {
            relativeOut = relStr;
            return true;
        }

        if (relStr == ".." || relStr.rfind("../", 0) == 0)
        {
            return false;
        }

        relativeOut = relStr;
        return true;
    }

    /**
     * Determines if a given pattern matches a relative path according to .gitignore-like rules. Supports
     * patterns that match file/dir names at any level, as well as anchored patterns with slashes.
     * @param pattern The ignore pattern to match (e.g., "*.tmp", "build/", "docs/*.log").
     * @param relativePath The path to test, relative to the repository root (e.g., "scratch.tmp",
     * "build/output.o", "docs/errors.log").
     * @return True if the pattern matches the relative path, false otherwise.
     */
    bool patternMatchesPath(const std::string &pattern, const std::string &relativePath)
    {
        std::filesystem::path relPath(relativePath);
        std::string fileName = relPath.filename().string();

        if (!pattern.empty() && pattern.back() == '/')
        {
            std::string dirPattern = pattern.substr(0, pattern.size() - 1);
            if (dirPattern.empty())
            {
                return false;
            }
            return relativePath == dirPattern || relativePath.rfind(dirPattern + "/", 0) == 0;
        }

        // Match anchored-ish paths when a slash is present.
        if (pattern.find('/') != std::string::npos)
        {
            return wildcardMatch(pattern, relativePath);
        }

        // Gitignore-like behavior: pattern without slash matches file/dir names at any level.
        if (wildcardMatch(pattern, fileName))
        {
            return true;
        }

        for (const auto &part : relPath)
        {
            if (wildcardMatch(pattern, part.string()))
            {
                return true;
            }
        }

        return false;
    }

    /**
     * Determines if a given file path should be ignored based on the patterns in .bvcsignore. The file path
     * should be an absolute path; the function will compute its path relative to the repo root and match against
     * the ignore patterns.
     * @param repoRoot The root directory of the repository.
     * @param filePath The file path to test.
     * @return True if the file should be ignored, false otherwise.
     */
    bool isIgnoredByBvcsIgnore(const std::filesystem::path &repoRoot, const std::filesystem::path &filePath)
    {
        std::filesystem::path ignoreFile = repoRoot / ".bvcsignore";
        if (!std::filesystem::exists(ignoreFile))
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::path absRepo = std::filesystem::weakly_canonical(repoRoot, ec);
        if (ec)
        {
            return false;
        }

        std::filesystem::path absFile = std::filesystem::weakly_canonical(filePath, ec);
        if (ec)
        {
            return false;
        }

        std::string relativePath;
        if (!isRelativeTo(absRepo, absFile, relativePath))
        {
            return false;
        }

        std::ifstream in(ignoreFile);
        if (!in)
        {
            throw std::runtime_error("Failed to open .bvcsignore: " + ignoreFile.string());
        }

        bool ignored = false;
        std::string rawLine;
        while (std::getline(in, rawLine))
        {
            std::string pattern = trim(rawLine);
            if (pattern.empty() || pattern[0] == '#')
            {
                continue;
            }

            bool negate = false;
            if (pattern[0] == '!')
            {
                negate = true;
                pattern = trim(pattern.substr(1));
                if (pattern.empty())
                {
                    continue;
                }
            }

            if (patternMatchesPath(pattern, relativePath))
            {
                ignored = !negate;
            }
        }

        return ignored;
    }

    /**
     * Escapes characters required for safe JSON string output.
     */
    std::string escapeJson(const std::string &input)
    {
        std::string out;
        out.reserve(input.size());
        for (char ch : input)
        {
            switch (ch)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += ch;
                break;
            }
        }
        return out;
    }
}

/**
 * Constructs a Repository rooted at the given path. Does not initialize anythin on the disk. Need to
 * call init() for that.
 * @param repoRoot The root directory of the repository.
 * @throws std::runtime_error if the repoRoot is not a valid directory.
 */
Repository::Repository(const std::filesystem::path &repoRoot) : m_root(repoRoot) {}

// Public Interface

/**
 * Creates the .bvcs directory structure on disk
 * @throws std::runtime_error if the repository is already initialized at the given path or if there
 * is an error creating the directory structure.
 */
void Repository::init()
{
    if (isInitialized())
    {
        throw std::runtime_error("Repository already exists at: " + m_root.string());
    }

    // Create the directory structure
    std::filesystem::create_directories(bvcsDir() / "objects");
    std::filesystem::create_directories(refsDir());

    // Write an empty HEAD file to mark the repo as initialized but
    // with no commits yet
    writeFileAtomic(bvcsDir() / "HEAD", {});

    std::cout << "Initialized empty repository at " << m_root.string() << "\n";
}

/**
 * Hashes the file at filePath and writes the hash to the index
 * @param filePath The path to the file to add to the staging area.
 * @throws std::runtime_error if the file does not exist or the repo is not initialized
 */
void Repository::add(const std::filesystem::path &filePath)
{
    assertInitialized();

    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("File not found: " + filePath.string());
    }

    if (isIgnoredByBvcsIgnore(m_root, filePath))
    {
        throw std::runtime_error("Refusing to add ignored file: " + filePath.string());
    }

    // Store the blob and record its hash in the index
    std::string hash = storeFromFile(m_root, filePath);

    std::string stagedHash = getStagedHash(m_root);
    if (!stagedHash.empty() && stagedHash != hash)
    {
        std::cerr << "warning: replacing previously staged file\n";
    }

    stage(m_root, hash);

    std::cout << "Staged " << filePath.filename().string()
              << " (" << hash.substr(0, 8) << "...)\n";
}

/**
 * Creates a commit from the currently staged hash, writes it to the object store, and advances HEAD
 * @param message The commit message describing the changes in this commit.
 * @throws std::runtime_error if there is no staged hash, the repo is not initialized
 */
void Repository::commit(const std::string &message)
{
    assertInitialized();
    assertStaged();

    if (message.empty())
    {
        throw std::runtime_error("Commit message cannot be empty.");
    }

    // Build the commit object
    Commit c;
    c.parentHash = headHash(); // empty string if this is the first commit
    c.blobHash = getStagedHash(m_root);
    c.message = message;
    c.timestamp = std::time(nullptr);

    // Serialize and store the commit object — its hash is its identity
    std::vector<unsigned char> serialized = serializeCommit(c);
    c.hash = storeFromMemory(m_root, serialized);
    writeHead(c.hash);
    clear(m_root);

    std::cout << "Committed " << c.hash.substr(0, 8) << "... \""
              << message << "\"\n";
}

/**
 * Walks the commit chain from HEAD and prints each commit's metadata to stdout.
 */
void Repository::log(bool jsonFormat) const
{
    assertInitialized();

    std::string hash = headHash();
    if (hash.empty())
    {
        if (jsonFormat)
        {
            std::cout << "{\"commits\":[]}\n";
        }
        else
        {
            std::cout << "No commits yet.\n";
        }
        return;
    }

    if (jsonFormat)
    {
        std::cout << "{\"commits\":[";
    }

    bool first = true;

    // Walk the parent chain until we reach a commit with no parent
    while (!hash.empty())
    {
        std::filesystem::path commitPath = objectPath(m_root, hash);
        std::vector<unsigned char> data = readFileBinary(commitPath);
        Commit c = deserializeCommit(data);
        c.hash = hash;

        if (jsonFormat)
        {
            if (!first)
            {
                std::cout << ",";
            }
            std::cout << "{"
                      << "\"hash\":\"" << c.hash << "\","
                      << "\"parent_hash\":\"" << c.parentHash << "\","
                      << "\"blob_hash\":\"" << c.blobHash << "\","
                      << "\"timestamp\":" << c.timestamp << ","
                      << "\"date\":\"" << escapeJson(formatTimestamp(c.timestamp)) << "\","
                      << "\"message\":\"" << escapeJson(c.message) << "\""
                      << "}";
            first = false;
        }
        else
        {
            std::cout << "commit " << c.hash << "\n"
                      << "date:   " << formatTimestamp(c.timestamp) << "\n"
                      << "        " << c.message << "\n\n";
        }

        hash = c.parentHash;
    }

    if (jsonFormat)
    {
        std::cout << "]}\n";
    }
}

/**
 * Retrieves the file associated with the given commit hash (or branch name) and writes it to outputPath
 * @param hashOrBranch The commit hash or branch name to check out.
 * @param outputPath The path where the checked-out file will be written.
 */
void Repository::checkout(const std::string &hashOrBranch, const std::filesystem::path &outputPath) const
{
    assertInitialized();

    std::string hash = resolveRef(hashOrBranch);

    // Read the commit to find its blob hash
    std::filesystem::path commitPath = objectPath(m_root, hash);
    if (!std::filesystem::exists(commitPath))
    {
        throw std::runtime_error("Commit not found: " + hash);
    }

    std::vector<unsigned char> data = readFileBinary(commitPath);
    Commit c = deserializeCommit(data);

    // Retrieve the blob from the object store
    retrieveToFile(m_root, c.blobHash, outputPath);

    std::cout << "Checked out " << hash.substr(0, 8) << "... to "
              << outputPath.string() << "\n";
}

/**
 * @return The root path of this repository.
 */
std::filesystem::path Repository::root() const
{
    return m_root;
}

/**
 * @return true if a .bvcs directory exists at the repoRoot
 */
bool Repository::isInitialized() const
{
    return std::filesystem::exists(bvcsDir());
}

// Private Helpers

/**
 * @return the path to .bvcs directory
 */
std::filesystem::path Repository::bvcsDir() const
{
    return m_root / ".bvcs";
}

/**
 * @return the path to .bvcs/refs/heads/
 */
std::filesystem::path Repository::refsDir() const
{
    return bvcsDir() / "refs" / "heads";
}

/**
 * @return The full SHA-256 hash that HEAD currently points to, or an empty string if the repo has no commits yet.
 */
std::string Repository::headHash() const
{
    std::filesystem::path headPath = bvcsDir() / "HEAD";
    if (!std::filesystem::exists(headPath))
    {
        return "";
    }
    std::vector<unsigned char> data = readFileBinary(headPath);
    return std::string(data.begin(), data.end());
}

/**
 * Writes a commit hash to .bvcs/HEAD
 * @param hash The commit hash to write to HEAD.
 */
void Repository::writeHead(const std::string &hash)
{
    std::vector<unsigned char> data(hash.begin(), hash.end());
    writeFileAtomic(bvcsDir() / "HEAD", data);
}

/**
 * Given a branch name or a hash prefix/full hash, returns the full resolved commit hash
 * @throws std::runtime_error if the name cannot be resolved to a valid commit hash
 * @param hashOrBranch The branch name or hash prefix/full hash to resolve.
 * @return The full commit hash that the name resolves to.
 */
std::string Repository::resolveRef(const std::string &hashOrBranch) const
{
    // Check if it's a branch name first
    std::filesystem::path branchPath = refsDir() / hashOrBranch;
    if (std::filesystem::exists(branchPath))
    {
        std::vector<unsigned char> data = readFileBinary(branchPath);
        return std::string(data.begin(), data.end());
    }

    // Otherwise treat it as a full or partial commit hash
    // For now require a full hash — partial hash resolution can be added later
    if (hashOrBranch.length() < 4)
    {
        throw std::runtime_error("Hash too short to resolve: " + hashOrBranch);
    }

    // If a full hash is provided and exists, use it directly.
    if (exists(m_root, hashOrBranch))
    {
        return hashOrBranch;
    }

    // Otherwise resolve it as a hash prefix by scanning the object store.
    std::vector<std::string> matches;
    std::filesystem::path objectsRoot = bvcsDir() / "objects";
    if (std::filesystem::exists(objectsRoot))
    {
        for (const auto &dirEntry : std::filesystem::directory_iterator(objectsRoot))
        {
            if (!dirEntry.is_directory())
            {
                continue;
            }

            std::string shard = dirEntry.path().filename().string();
            if (shard.length() != 2)
            {
                continue;
            }

            for (const auto &fileEntry : std::filesystem::directory_iterator(dirEntry.path()))
            {
                if (!fileEntry.is_regular_file())
                {
                    continue;
                }

                std::string suffix = fileEntry.path().filename().string();
                std::string fullHash = shard + suffix;
                if (fullHash.rfind(hashOrBranch, 0) == 0)
                {
                    // Restrict checkout ref resolution to commit-like objects.
                    try
                    {
                        std::vector<unsigned char> data = readFileBinary(fileEntry.path());
                        (void)deserializeCommit(data);
                        matches.push_back(fullHash);
                    }
                    catch (...)
                    {
                        // Not a commit object; ignore for checkout ref resolution.
                    }
                }
            }
        }
    }

    if (matches.empty())
    {
        throw std::runtime_error("Could not resolve ref: " + hashOrBranch);
    }

    if (matches.size() > 1)
    {
        throw std::runtime_error("Ambiguous ref prefix: " + hashOrBranch);
    }

    return matches.front();
}

/**
 * @throws std::runtime_error if the repository is not initialized
 */
void Repository::assertInitialized() const
{
    if (!isInitialized())
    {
        throw std::runtime_error(
            "Not a bvcs repository. Run 'bvcs init' first.");
    }
}

/**
 * @throws std::runtime_error if there is no staged hash in the index
 */
void Repository::assertStaged() const
{
    if (!hasStaged(m_root))
    {
        throw std::runtime_error(
            "Nothing staged. Run 'bvcs add <file>' first.");
    }
}

/**
 * Compares three states: the last committed blob hash, the currently staged hash, and the hash of the file on disk right now
 */
void Repository::status(const std::filesystem::path &filePath, bool jsonFormat) const
{
    assertInitialized();

    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("File not found: " + filePath.string());
    }

    std::string headCommitHash = headHash();
    std::string stagedHash = getStagedHash(m_root);

    if (headCommitHash.empty())
    {
        if (jsonFormat)
        {
            std::cout << "{\"error\":\"No commits yet.\"}\n";
        }
        else
        {
            std::cout << "No commits yet.\n";
        }
        return;
    }

    std::filesystem::path commitPath = objectPath(m_root, headCommitHash);
    if (!std::filesystem::exists(commitPath))
    {
        throw std::runtime_error("HEAD commit object is missing: " + headCommitHash);
    }

    std::vector<unsigned char> commitData = readFileBinary(commitPath);
    Commit headCommit = deserializeCommit(commitData);
    std::string committedBlobHash = headCommit.blobHash;

    std::string workingHash = hashFile(filePath);
    std::string stagedState = "none";
    if (!stagedHash.empty())
    {
        stagedState = (committedBlobHash == stagedHash) ? "unchanged" : "modified";
    }

    std::string workingBaseline = stagedHash.empty() ? committedBlobHash : stagedHash;
    std::string workingState = (workingHash == workingBaseline) ? "unchanged" : "modified";

    if (jsonFormat)
    {
        std::cout << "{"
                  << "\"head_commit\":\"" << headCommitHash << "\","
                  << "\"committed_blob\":\"" << committedBlobHash << "\","
                  << "\"staged_blob\":";

        if (stagedHash.empty())
        {
            std::cout << "null";
        }
        else
        {
            std::cout << "\"" << stagedHash << "\"";
        }

        std::cout << ",\"staged_state\":\"" << stagedState << "\","
                  << "\"working_blob\":\"" << workingHash << "\","
                  << "\"working_state\":\"" << workingState << "\""
                  << "}\n";
        return;
    }

    std::cout << "On commit: " << committedBlobHash.substr(0, 8) << "...\n";

    if (!stagedHash.empty())
    {
        std::cout << "Staged:    " << stagedHash.substr(0, 8) << "... ";
        if (committedBlobHash == stagedHash)
            std::cout << "(unchanged)\n";
        else
            std::cout << "(modified)\n";
    }
    else
    {
        std::cout << "Staged:    " << "(none)\n";
    }

    std::cout << "Working:   " << workingHash.substr(0, 8) << "... ";
    if (workingState == "unchanged")
    {
        std::cout << "(unchanged)\n";
    }
    else
    {
        std::cout << "(modified)\n";
    }
}