// defines the Commit struct (hash, parent hash, blob hash, timestamp, message) and the functions
// to serialize/deserialize it to/from bytes. No file I/O here — it only knows how to turn a Commit
// into a byte sequence and back. The actual writing to disk goes through ObjectStore.

// Commit { string hash, parent, blobHash, message; time_t timestamp; }
// serialize(commit) -> vector<byte>
// deserialize(bytes) -> Commit

#include "commit.h"
#include <sstream>

/**
 * Serializes a Commit object into a byte vector. The serialization format is:
 * parentHash\nblobHash\ntimestamp\nmessage
 * The commit hash is not included in the serialized data since it is computed from the contents.
 * @param commit The Commit object to serialize.
 * @return A vector of bytes representing the serialized commit.
 */
std::vector<unsigned char> serializeCommit(const Commit &commit)
{
    std::ostringstream oss;
    oss << commit.parentHash << '\0'
        << commit.blobHash << '\0'
        << commit.timestamp << '\0'
        << commit.message;

    std::string str = oss.str();
    return std::vector<unsigned char>(str.begin(), str.end());
}

/**
 * Deserializes a byte vector into a Commit object. The deserialization format is:
 * parentHash\0blobHash\0timestamp\0message
 * @param data The byte vector representing the serialized commit.
 * @return The deserialized Commit object.
 */
Commit deserializeCommit(const std::vector<unsigned char> &data)
{
    std::string str(data.begin(), data.end());
    std::istringstream iss(str);
    std::string parentHash, blobHash, message;
    time_t timestamp;

    std::getline(iss, parentHash);
    std::getline(iss, blobHash);
    iss >> timestamp;
    // iss.ignore(); // Ignore the null terminator
    if (!(iss) >> timestamp) {
        throw std::runtime_error("Failed to parse timestamp from commit data.");
    }

    std::getline(iss, message);

    return Commit{std::string(), parentHash, blobHash, message, timestamp};
}