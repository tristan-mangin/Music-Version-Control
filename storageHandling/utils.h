// anything that doesn't belong to a specific domain but is used in multiple files. Reading 
// a whole file into a vector<byte>, writing bytes atomically via temp file + rename, 
// formatting a timestamp as a string.

// readFileBinary(path) -> vector<byte>
// writeFileAtomic(path, data)
// formatTimestamp(time_t) -> string
// hexToBytes(hex) -> vector<byte>