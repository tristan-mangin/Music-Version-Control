// the orchestrator. This is the only file that knows about the whole system and calls the 
// others. Each user command (init, add, commit, log, checkout) is a method here. It holds 
// a path to the repo root and creates the ObjectStore, Index, etc. as member objects.

// Class: Repository 
//      public:
//          init(), add(filePath), commit(message), log(), checkout(hashOrBranch, outputPath)
//      private:
//          resolveRef(name) -> hash, headHash() -> string, writeHead(hash), findRepoRoot() -> path