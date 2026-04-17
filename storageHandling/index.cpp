// manages the staging area, which is just a small file at .bvcs/index that holds the
// hash of whatever file has been added but not yet committed. Reads and writes that one file.

// stage(hash)
// getStagedHash() -> string
// clear()
// hasStaged() -> bool

#include "index.h"
#include <string>
#include <fstream>
#include <stdexcept>
#include <filesystem>

/**
 * Writes the given hash to the staging area file (.storageHandling/index). This function overwrites any existing content in the index file.
 * @param hash The hash string to stage. This should be the hash of the file that has been added but not yet committed.
 * @throws std::runtime_error if the index file cannot be opened for writing.
 */
void stage(const std::filesystem::path &repoRoot, const std::string &hash)
{
    std::filesystem::path indexPath = repoRoot / ".storageHandling" / "index";
    std::vector<unsigned char> data(hash.begin(), hash.end());
    writeFileAtomic(indexPath, data);
}

/**
 * Reads and returns the hash currently staged in the index file. If the index file does not exist or is empty, this function returns an empty string.
 * @return The hash string currently staged, or an empty string if no hash is staged.
 * @throws std::runtime_error if the index file cannot be opened for reading.
 */
std::string getStagedHash(const std::filesystem::path &repoRoot)
{
    std::filesystem::path indexPath = repoRoot / ".storageHandling" / "index";
    if (!std::filesystem::exists(indexPath))
    {
        return "";
    }
    std::ifstream ifs(indexPath);
    if (!ifs)
    {
        throw std::runtime_error("Failed to open index file for reading.");
    }
    std::string hash;
    std::getline(ifs, hash);
    return hash;
}

/**
 * Clears the staging area by deleting the index file. If the file does not exist, this function does nothing.
 * @throws std::runtime_error if the index file cannot be deleted.
 */
void clear(const std::filesystem::path &repoRoot)
{
    std::filesystem::path indexPath = repoRoot / ".storageHandling" / "index";
    if (std::filesystem::exists(indexPath))
    {
        std::filesystem::remove(indexPath);
    }
}

/**
 * Checks if there is a staged hash in the index file.
 * @return true if there is a staged hash, false otherwise.
 */
bool hasStaged(const std::filesystem::path &repoRoot)
{
    return !getStagedHash(repoRoot).empty();
}