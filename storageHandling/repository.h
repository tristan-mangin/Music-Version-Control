// the orchestrator. This is the only file that knows about the whole system and calls the
// others. Each user command (init, add, commit, log, checkout) is a method here. It holds
// a path to the repo root and creates the ObjectStore, Index, etc. as member objects.

// Class: Repository
//      public:
//          init(), add(filePath), commit(message), log(), checkout(hashOrBranch, outputPath)
//      private:
//          resolveRef(name) -> hash, headHash() -> string, writeHead(hash), findRepoRoot() -> path

#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "commit.h"
#include "index.h"
#include "object_store.h"

/**
 * Serves as the main interface for interacting with the version control system.
 * It provides methods for initializing a repository, adding files, committing changes, viewing commit logs,
 * and checking out specific commits or branches. The Repository class manages the internal workings of the
 * system by coordinating between the Commit, Index, and ObjectStore components.
 * @param repoRoot The root directory of the repository, where all version control data is stored.
 * This is determined when the Repository object is created and is used as the base path for all operations.
 */
class Repository
{
public:
    explicit Repository(const std::filesystem::path &repoRoot);
    void init();
    void add(const std::filesystem::path &filePath);
    void commit(const std::string &message);
    void log(bool jsonFormat = false) const;
    void checkout(const std::string &hashOrBranch, const std::filesystem::path &outputPath) const;
    void status(const std::filesystem::path &filePath, bool jsonFormat = false) const;
    std::filesystem::path root() const;
    bool isInitialized() const;

private:
    std::filesystem::path m_root;

    std::filesystem::path bvcsDir() const;
    std::filesystem::path refsDir() const;
    std::string headHash() const;
    void writeHead(const std::string &hash);
    std::string resolveRef(const std::string &hashOrBranch) const;
    void assertInitialized() const;
    void assertStaged() const;
};

#endif // REPOSITORY_H