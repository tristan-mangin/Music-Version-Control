// take a byte buffer or a file path and return a hex string hash. It knows nothing about files 
// on disk beyond reading bytes. Everything else that needs a hash calls this. Keeping it isolated 
// means you can swap SHA-256 for something else (xxHash, BLAKE3) by changing exactly one file.

// hashFile(path) -> string
// hashBytes(buffer, size) -> string

#ifndef HASHER_H
#define HASHER_H

#include <string>
#include <filesystem>

std::string hashFile(const std::filesystem::path& filePath);
std::string hashBytes(const unsigned char* buffer, size_t size);

#endif // HASHER_H