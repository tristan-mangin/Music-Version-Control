// manages the staging area, which is just a small file at .bvcs/index that holds the 
// hash of whatever file has been added but not yet committed. Reads and writes that one file.

// stage(hash)
// getStagedHash() -> string
// clear()
// hasStaged() -> bool