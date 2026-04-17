#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include "../storageHandling/repository.h"

// helpers 

int passed = 0;
int failed = 0;

void check(const std::string &testName, bool condition)
{
    if (condition)
    {
        std::cout << "[PASS] " << testName << "\n";
        passed++;
    }
    else
    {
        std::cout << "[FAIL] " << testName << "\n";
        failed++;
    }
}

// Each test gets a completely isolated directory. The destructor cleans
// everything up so no test state leaks into another.
struct TempRepo
{
    std::filesystem::path root;
    Repository repo;

    TempRepo()
        : root(std::filesystem::temp_directory_path() / "bvcs_repo_test"),
          repo(root)
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
    }

    ~TempRepo()
    {
        std::filesystem::remove_all(root);
    }

    // Writes a file inside the repo working directory with given contents
    std::filesystem::path makeFile(const std::string &name,
                                   const std::string &contents)
    {
        std::filesystem::path path = root / name;
        std::ofstream out(path, std::ios::binary);
        out.write(contents.data(), contents.size());
        return path;
    }

    // Writes a file with raw binary contents
    std::filesystem::path makeBinaryFile(const std::string &name,
                                          const std::vector<unsigned char> &contents)
    {
        std::filesystem::path path = root / name;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(contents.data()), contents.size());
        return path;
    }

    // Reads a file back as a string for comparison
    std::string readFile(const std::filesystem::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    }
};

// init 

void test_init_createsBvcsDirectory()
{
    TempRepo t;
    t.repo.init();
    check("init: .bvcs directory is created",
          std::filesystem::exists(t.root / ".bvcs"));
}

void test_init_createsObjectsDirectory()
{
    TempRepo t;
    t.repo.init();
    check("init: .bvcs/objects directory is created",
          std::filesystem::exists(t.root / ".bvcs" / "objects"));
}

void test_init_createsRefsDirectory()
{
    TempRepo t;
    t.repo.init();
    check("init: .bvcs/refs/heads directory is created",
          std::filesystem::exists(t.root / ".bvcs" / "refs" / "heads"));
}

void test_init_createsHEADFile()
{
    TempRepo t;
    t.repo.init();
    check("init: .bvcs/HEAD file is created",
          std::filesystem::exists(t.root / ".bvcs" / "HEAD"));
}

void test_init_throwsIfAlreadyInitialized()
{
    TempRepo t;
    t.repo.init();
    bool threw = false;
    try { t.repo.init(); }
    catch (const std::runtime_error &) { threw = true; }
    check("init: throws if repository already exists", threw);
}

void test_init_isInitializedReturnsTrueAfterInit()
{
    TempRepo t;
    check("isInitialized: returns false before init", !t.repo.isInitialized());
    t.repo.init();
    check("isInitialized: returns true after init", t.repo.isInitialized());
}

// add ─

void test_add_throwsIfNotInitialized()
{
    TempRepo t;
    auto file = t.makeFile("test.wav", "audio data");
    bool threw = false;
    try { t.repo.add(file); }
    catch (const std::runtime_error &) { threw = true; }
    check("add: throws if repo is not initialized", threw);
}

void test_add_throwsIfFileDoesNotExist()
{
    TempRepo t;
    t.repo.init();
    bool threw = false;
    try { t.repo.add(t.root / "nonexistent.wav"); }
    catch (const std::runtime_error &) { threw = true; }
    check("add: throws if file does not exist", threw);
}

void test_add_stagesFileHash()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");
    t.repo.add(file);
    check("add: file hash is staged after add",
          hasStaged(t.root));
}

void test_add_storesBlob()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");
    t.repo.add(file);

    // The staged hash should correspond to a real object on disk
    std::string hash = getStagedHash(t.root);
    check("add: blob is stored in object store", exists(t.root, hash));
}

void test_add_addingSameFileTwiceDeduplicates()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");

    t.repo.add(file);
    std::string firstHash = getStagedHash(t.root);

    t.repo.add(file);
    std::string secondHash = getStagedHash(t.root);

    check("add: adding same file twice produces the same hash",
          firstHash == secondHash);
}

void test_add_addingDifferentFilesProducesDifferentHashes()
{
    TempRepo t;
    t.repo.init();

    auto fileA = t.makeFile("a.wav", "version one");
    t.repo.add(fileA);
    std::string hashA = getStagedHash(t.root);

    auto fileB = t.makeFile("b.wav", "version two");
    t.repo.add(fileB);
    std::string hashB = getStagedHash(t.root);

    check("add: different files produce different staged hashes", hashA != hashB);
}

// commit 

void test_commit_throwsIfNotInitialized()
{
    TempRepo t;
    bool threw = false;
    try { t.repo.commit("initial commit"); }
    catch (const std::runtime_error &) { threw = true; }
    check("commit: throws if repo is not initialized", threw);
}

void test_commit_throwsIfNothingStaged()
{
    TempRepo t;
    t.repo.init();
    bool threw = false;
    try { t.repo.commit("initial commit"); }
    catch (const std::runtime_error &) { threw = true; }
    check("commit: throws if nothing is staged", threw);
}

void test_commit_throwsOnEmptyMessage()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");
    t.repo.add(file);
    bool threw = false;
    try { t.repo.commit(""); }
    catch (const std::runtime_error &) { threw = true; }
    check("commit: throws on empty message", threw);
}

void test_commit_clearsIndexAfterCommit()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");
    t.repo.add(file);
    t.repo.commit("initial commit");
    check("commit: index is cleared after commit", !hasStaged(t.root));
}

void test_commit_advancesHEAD()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");
    t.repo.add(file);
    t.repo.commit("initial commit");

    std::filesystem::path headPath = t.root / ".bvcs" / "HEAD";
    std::string head = t.readFile(headPath);
    check("commit: HEAD is non-empty after commit", !head.empty());
}

void test_commit_secondCommitHasDifferentHash()
{
    TempRepo t;
    t.repo.init();

    auto fileA = t.makeFile("a.wav", "first version");
    t.repo.add(fileA);
    t.repo.commit("first commit");
    std::string firstHead = t.readFile(t.root / ".bvcs" / "HEAD");

    auto fileB = t.makeFile("b.wav", "second version");
    t.repo.add(fileB);
    t.repo.commit("second commit");
    std::string secondHead = t.readFile(t.root / ".bvcs" / "HEAD");

    check("commit: successive commits produce different hashes",
          firstHead != secondHead);
}

void test_commit_commitObjectStoredInObjectStore()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");
    t.repo.add(file);
    t.repo.commit("initial commit");

    std::string hash = t.readFile(t.root / ".bvcs" / "HEAD");
    check("commit: commit object exists in object store", exists(t.root, hash));
}

// log 

void test_log_throwsIfNotInitialized()
{
    TempRepo t;
    bool threw = false;
    try { t.repo.log(); }
    catch (const std::runtime_error &) { threw = true; }
    check("log: throws if repo is not initialized", threw);
}

void test_log_doesNotThrowOnEmptyRepo()
{
    TempRepo t;
    t.repo.init();
    bool threw = false;
    try { t.repo.log(); }
    catch (...) { threw = true; }
    check("log: does not throw on repo with no commits", !threw);
}

void test_log_doesNotThrowWithCommits()
{
    TempRepo t;
    t.repo.init();
    auto file = t.makeFile("test.wav", "audio data");
    t.repo.add(file);
    t.repo.commit("initial commit");

    bool threw = false;
    try { t.repo.log(); }
    catch (...) { threw = true; }
    check("log: does not throw with commits present", !threw);
}

// checkout 

void test_checkout_throwsIfNotInitialized()
{
    TempRepo t;
    bool threw = false;
    try { t.repo.checkout("abc123", t.root / "output.wav"); }
    catch (const std::runtime_error &) { threw = true; }
    check("checkout: throws if repo is not initialized", threw);
}

void test_checkout_throwsOnUnknownHash()
{
    TempRepo t;
    t.repo.init();
    bool threw = false;
    std::string fakeHash = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    try { t.repo.checkout(fakeHash, t.root / "output.wav"); }
    catch (const std::runtime_error &) { threw = true; }
    check("checkout: throws on unknown hash", threw);
}

void test_checkout_restoresOriginalFileContents()
{
    TempRepo t;
    t.repo.init();
    std::string originalContents = "original audio data";
    auto file = t.makeFile("test.wav", originalContents);
    t.repo.add(file);
    t.repo.commit("initial commit");

    std::string commitHash = t.readFile(t.root / ".bvcs" / "HEAD");
    auto outputPath = t.root / "restored.wav";
    t.repo.checkout(commitHash, outputPath);

    check("checkout: restored file matches original contents",
          t.readFile(outputPath) == originalContents);
}

void test_checkout_restoresBinaryFileContents()
{
    TempRepo t;
    t.repo.init();
    // Simulate WAV header bytes
    std::vector<unsigned char> wavContents = {
        0x52, 0x49, 0x46, 0x46, 0x00, 0xFF, 0x80, 0x01
    };
    auto file = t.makeBinaryFile("test.wav", wavContents);
    t.repo.add(file);
    t.repo.commit("binary commit");

    std::string commitHash = t.readFile(t.root / ".bvcs" / "HEAD");
    auto outputPath = t.root / "restored.wav";
    t.repo.checkout(commitHash, outputPath);

    std::ifstream in(outputPath, std::ios::binary);
    std::vector<unsigned char> restored(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    check("checkout: binary file contents are restored exactly",
          restored == wavContents);
}

void test_checkout_canRetrieveEarlierVersion()
{
    TempRepo t;
    t.repo.init();

    // Commit version one
    auto fileV1 = t.makeFile("track.wav", "version one audio");
    t.repo.add(fileV1);
    t.repo.commit("version one");
    std::string hashV1 = t.readFile(t.root / ".bvcs" / "HEAD");

    // Commit version two
    auto fileV2 = t.makeFile("track_v2.wav", "version two audio");
    t.repo.add(fileV2);
    t.repo.commit("version two");

    // Check out version one by its hash
    auto outputPath = t.root / "retrieved_v1.wav";
    t.repo.checkout(hashV1, outputPath);

    check("checkout: can retrieve an earlier version by commit hash",
          t.readFile(outputPath) == "version one audio");
}

// full workflow 

void test_fullWorkflow_initAddCommitCheckout()
{
    TempRepo t;
    std::string contents = "a full workflow test with some audio-like content";

    t.repo.init();
    auto file = t.makeFile("project.wav", contents);
    t.repo.add(file);
    t.repo.commit("full workflow commit");

    std::string commitHash = t.readFile(t.root / ".bvcs" / "HEAD");
    auto outputPath = t.root / "output.wav";
    t.repo.checkout(commitHash, outputPath);

    check("full workflow: init -> add -> commit -> checkout produces original file",
          t.readFile(outputPath) == contents);
}

void test_fullWorkflow_multipleVersionsAllRetrievable()
{
    TempRepo t;
    t.repo.init();

    std::vector<std::string> versions = {
        "first recording take",
        "second recording take",
        "third recording take"
    };
    std::vector<std::string> hashes;

    // Commit all three versions
    for (size_t i = 0; i < versions.size(); ++i)
    {
        auto file = t.makeFile("take_" + std::to_string(i) + ".wav", versions[i]);
        t.repo.add(file);
        t.repo.commit("take " + std::to_string(i + 1));
        hashes.push_back(t.readFile(t.root / ".bvcs" / "HEAD"));
    }

    // Verify each version can be independently retrieved
    bool allCorrect = true;
    for (size_t i = 0; i < versions.size(); ++i)
    {
        auto outputPath = t.root / ("retrieved_" + std::to_string(i) + ".wav");
        t.repo.checkout(hashes[i], outputPath);
        if (t.readFile(outputPath) != versions[i])
        {
            allCorrect = false;
        }
    }
    check("full workflow: all committed versions are independently retrievable",
          allCorrect);
}

// main 

int main()
{
    std::cout << "=== repository tests ===\n\n";

    test_init_createsBvcsDirectory();
    test_init_createsObjectsDirectory();
    test_init_createsRefsDirectory();
    test_init_createsHEADFile();
    test_init_throwsIfAlreadyInitialized();
    test_init_isInitializedReturnsTrueAfterInit();

    test_add_throwsIfNotInitialized();
    test_add_throwsIfFileDoesNotExist();
    test_add_stagesFileHash();
    test_add_storesBlob();
    test_add_addingSameFileTwiceDeduplicates();
    test_add_addingDifferentFilesProducesDifferentHashes();

    test_commit_throwsIfNotInitialized();
    test_commit_throwsIfNothingStaged();
    test_commit_throwsOnEmptyMessage();
    test_commit_clearsIndexAfterCommit();
    test_commit_advancesHEAD();
    test_commit_secondCommitHasDifferentHash();
    test_commit_commitObjectStoredInObjectStore();

    test_log_throwsIfNotInitialized();
    test_log_doesNotThrowOnEmptyRepo();
    test_log_doesNotThrowWithCommits();

    test_checkout_throwsIfNotInitialized();
    test_checkout_throwsOnUnknownHash();
    test_checkout_restoresOriginalFileContents();
    test_checkout_restoresBinaryFileContents();
    test_checkout_canRetrieveEarlierVersion();

    test_fullWorkflow_initAddCommitCheckout();
    test_fullWorkflow_multipleVersionsAllRetrievable();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}