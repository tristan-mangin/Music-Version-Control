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
void Repository::log() const
{
    assertInitialized();

    std::string hash = headHash();
    if (hash.empty())
    {
        std::cout << "No commits yet.\n";
        return;
    }

    // Walk the parent chain until we reach a commit with no parent
    while (!hash.empty())
    {
        std::filesystem::path commitPath = objectPath(m_root, hash);
        std::vector<unsigned char> data = readFileBinary(commitPath);
        Commit c = deserializeCommit(data);
        c.hash = hash;

        std::cout << "commit " << c.hash << "\n"
                  << "date:   " << formatTimestamp(c.timestamp) << "\n"
                  << "        " << c.message << "\n\n";

        hash = c.parentHash;
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
void Repository::status(const std::filesystem::path &filePath) const
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
        std::cout << "No commits yet.\n";
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
    std::string workingBaseline = stagedHash.empty() ? committedBlobHash : stagedHash;
    if (workingHash == workingBaseline)
    {
        std::cout << "(unchanged)\n";
    }
    else
    {
        std::cout << "(modified)\n";
    }
}