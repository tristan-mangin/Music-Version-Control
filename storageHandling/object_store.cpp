// responsible for reading and writing blobs to objects/. It takes a hash and gives back bytes,
// or takes bytes and returns a hash. It decides the objects/ab/cdef123... directory sharding
// scheme. It does not know what a commit is — it just stores and retrieves opaque byte sequences.

// storeFromFile(filePath) -> hash_string
// retrieveToFile(hash, filePath) -> void
// exists(hash) -> bool
// objectPath(hash) -> filesystem::path

#include "object_store.h"
#include <fstream>
#include <stdexcept>

/**
 * Stores the contents of a file in the object store and returns its hash. If an object with the same
 * hash already exists, it does not store it again.
 * @param filePath The path to the file to store.
 * @return The hash of the file's contents as a hex string.
 * @throws std::runtime_error if the file cannot be read or written to the object store.
 */
std::string storeFromFile(const std::filesystem::path &repoRoot, const std::filesystem::path &filePath)
{
    std::string hash = hashFile(filePath);
    std::filesystem::path destPath = objectPath(repoRoot, hash);

    // If the object already exists, we don't need to store it again
    if (!std::filesystem::exists(destPath))
    {
        // Ensure the parent directory exists and copy the file to the object store location
        std::filesystem::create_directories(destPath.parent_path());
        try
        {
            std::filesystem::copy_file(filePath, destPath);
        }
        catch (...)
        {
            std::filesystem::remove(destPath);
            throw;
        }
    }

    return hash;
}

/**
 * Retrieves the contents of an object identified by its hash and writes it to a file.
 * @param hash The hash of the object to retrieve.
 * @param filePath The path to the file where the object's contents will be written.
 * @throws std::runtime_error if the object does not exist in the store.
 * @throws std::runtime_error if the file cannot be written.
 */
void retrieveToFile(const std::filesystem::path &repoRoot, const std::string &hash, const std::filesystem::path &filePath)
{
    std::filesystem::path srcPath = objectPath(repoRoot, hash);
    if (!exists(repoRoot, hash))
    {
        throw std::runtime_error("Object not found: " + hash);
    }

    try
    {
        std::filesystem::copy_file(srcPath, filePath, std::filesystem::copy_options::overwrite_existing);
    }
    catch (...)
    {
        std::filesystem::remove(filePath);
        throw;
    }
}

/**
 * Checks if an object with the given hash exists in the object store.
 * @param hash The hash of the object to check for existence.
 * @return True if the object exists, false otherwise.
 */
bool exists(const std::filesystem::path &repoRoot, const std::string &hash)
{
    return std::filesystem::exists(objectPath(repoRoot, hash));
}

/**
 * Given a hash string, returns the filesystem path where the corresponding object is stored.
 * @param hash The hash of the object.
 * @return The filesystem path to the object in the object store.
 * @throws std::runtime_error if the hash is too short to be valid (less than 4 characters).
 */
std::filesystem::path objectPath(const std::filesystem::path &repoRoot, const std::string &hash)
{
    if (hash.length() < 4)
    {
        throw std::runtime_error("Invalid hash: too short to be a valid SHA-256 hash");
    }
    // Use the first 2 characters of the hash as the directory and the rest as the filename
    std::string dir = hash.substr(0, 2);
    std::string filename = hash.substr(2);
    return repoRoot / ".bvcs" / "objects" / dir / filename;
}