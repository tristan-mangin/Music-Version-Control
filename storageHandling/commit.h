// defines the Commit struct (hash, parent hash, blob hash, timestamp, message) and the functions 
// to serialize/deserialize it to/from bytes. No file I/O here — it only knows how to turn a Commit 
// into a byte sequence and back. The actual writing to disk goes through ObjectStore.

// Commit { string hash, parent, blobHash, message; time_t timestamp; }
// serialize(commit) -> vector<byte>
// deserialize(bytes) -> Commit