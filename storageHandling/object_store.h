// responsible for reading and writing blobs to objects/. It takes a hash and gives back bytes, 
// or takes bytes and returns a hash. It decides the objects/ab/cdef123... directory sharding 
// scheme. It does not know what a commit is — it just stores and retrieves opaque byte sequences.

// storeFromFile(filePath) -> hash_string
// retrieveToFile(hash, filePath) -> void
// exists(hash) -> bool
// objectPath(hash) -> filesystem::path

#ifndef OBJECT_STORE_H
#define OBJECT_STORE_H

#include "hasher.h"
#include "utils.h"
#include <string>
#include <filesystem>

std::string storeFromFile(const std::filesystem::path &repoRoot, const std::filesystem::path &filePath);
void retrieveToFile(const std::filesystem::path &repoRoot, const std::string &hash, const std::filesystem::path &filePath);
bool exists(const std::filesystem::path &repoRoot, const std::string &hash);
std::filesystem::path objectPath(const std::filesystem::path &repoRoot, const std::string &hash);

#endif // OBJECT_STORE_H