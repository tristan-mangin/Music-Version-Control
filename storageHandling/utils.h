// anything that doesn't belong to a specific domain but is used in multiple files. Reading 
// a whole file into a vector<byte>, writing bytes atomically via temp file + rename, 
// formatting a timestamp as a string.

// readFileBinary(path) -> vector<byte>
// writeFileAtomic(path, data)
// formatTimestamp(time_t) -> string
// hexToBytes(hex) -> vector<byte>

#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <filesystem>

std::vector<unsigned char> readFileBinary(const std::filesystem::path &filePath);
void writeFileAtomic(const std::filesystem::path &filePath, const std::vector<unsigned char> &data);
std::string formatTimestamp(std::time_t timestamp);
std::vector<unsigned char> hexToBytes(const std::string &hex);
std::string toHexString(const unsigned char *data, size_t length);

#endif // UTILS_H