// defines the Commit struct (hash, parent hash, blob hash, timestamp, message) and the functions 
// to serialize/deserialize it to/from bytes. No file I/O here — it only knows how to turn a Commit 
// into a byte sequence and back. The actual writing to disk goes through ObjectStore.

// Commit { string hash, parent, blobHash, message; time_t timestamp; }
// serialize(commit) -> vector<byte>
// deserialize(bytes) -> Commit

#ifndef COMMIT_H
#define COMMIT_H

#include "hasher.h"
#include "utils.h"
#include <string>
#include <vector>

/**
 * Represents a commit in the version control system. Each commit has a unique hash, 
 * a reference to its parent commit, a reference to the blob (file contents) it points 
 * to, a commit message, and a timestamp.
 * @param hash The unique hash of the commit, computed from its contents.
 * @param parentHash The hash of the parent commit (empty string for the initial commit).
 * @param blobHash The hash of the blob (file contents) that this commit points to
 * @param message The commit message describing the changes in this commit.
 * @param timestamp The time when the commit was created, represented as a time_t value.
 */
struct Commit
{
    std::string hash;
    std::string parentHash;
    std::string blobHash;
    std::string message;
    time_t timestamp;
};

std::vector<unsigned char> serializeCommit(const Commit &commit);
Commit deserializeCommit(const std::vector<unsigned char> &data);

#endif // COMMIT_H