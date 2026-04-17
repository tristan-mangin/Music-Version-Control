// manages the staging area, which is just a small file at .bvcs/index that holds the 
// hash of whatever file has been added but not yet committed. Reads and writes that one file.

// stage(hash)
// getStagedHash() -> string
// clear()
// hasStaged() -> bool

#ifndef INDEX_H
#define INDEX_H

#include "hasher.h"
#include "utils.h"

void stage(const std::filesystem::path &repoRoot, const std::string &hash);
std::string getStagedHash(const std::filesystem::path &repoRoot);
void clear(const std::filesystem::path &repoRoot);
bool hasStaged(const std::filesystem::path &repoRoot);

#endif // INDEX_H