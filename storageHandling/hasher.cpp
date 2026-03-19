// take a byte buffer or a file path and return a hex string hash. It knows nothing about files
// on disk beyond reading bytes. Everything else that needs a hash calls this. Keeping it isolated
// means you can swap SHA-256 for something else (xxHash, BLAKE3) by changing exactly one file.

// hashFile(path) -> string
// hashBytes(buffer, size) -> string

#include "hasher.h"
#include <openssl/sha.h>
#include <fstream>

/**
 * Hashes the contents of a file and returns the hash as a hex string.
 * @param filePath The path to the file to hash.
 * @return The SHA-256 hash of the file's contents as a hex string.
 */
std::string hashFile(std::filesystem::path &filePath)
{
    // Open the file in binary mode so bytes are read correctly
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Could not open file: " + filePath.string());
    }

    // Create and initialize the SHA-256 context for hashing
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    // 8KB buffer for reading the file in chunks to be more memory efficient
    // Reads full buffers until it can't read a full one and updates hash with read bytes
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)))
    {
        SHA256_Update(&sha256, buffer, file.gcount());
    }
    // read the last chunk if there are still bytes left after the loop
    if (file.gcount() > 0)
    {
        SHA256_Update(&sha256, buffer, file.gcount());
    }

    // close the hash computation and write 32-byte digest into hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    // append 2-digit hex for each byte in the hash to a stringstream and return it as a string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

/**
 * takes a pointer to data already in memory with its size and returns the hash as a hex string.
 * @param buffer The byte buffer to hash.
 * @param size The size of the byte buffer.
 * @return The SHA-256 hash of the buffer as a hex string.
 */
std::string hashBytes(const unsigned char *buffer, size_t size) {
    // Create and initialize the SHA-256 context for hashing
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    // Update the hash with the provided buffer
    SHA256_Update(&sha256, buffer, size);

    // Finalize the hash computation and write 32-byte digest into hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);

    // Convert the hash bytes to a hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}