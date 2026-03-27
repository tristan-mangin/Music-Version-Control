#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include "../storageHandling/object_store.h"
#include "../storageHandling/hasher.h"

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

// Creates a self-cleaning temporary directory for each test to work in.
// This ensures tests don't interfere with each other and nothing is left
// behind on disk after the suite runs.
struct TempRepo
{
    std::filesystem::path root;

    TempRepo()
    {
        root = std::filesystem::temp_directory_path() / "bvcs_test_repo";
        std::filesystem::create_directories(root);
    }

    ~TempRepo()
    {
        std::filesystem::remove_all(root);
    }

    // Convenience: write a file inside the repo with given contents
    std::filesystem::path makeFile(const std::string &name, const std::vector<unsigned char> &contents)
    {
        std::filesystem::path path = root / name;
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(contents.data()), contents.size());
        return path;
    }
};

// objectPath 

void test_objectPath_correctStructure()
{
    TempRepo repo;
    std::string hash = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    std::filesystem::path result = objectPath(repo.root, hash);

    // Should be repoRoot/.bvcs/objects/ab/cdef1234...
    std::filesystem::path expected = repo.root / ".bvcs" / "objects" / "ab" / hash.substr(2);
    check("objectPath: produces correct directory structure", result == expected);
}

void test_objectPath_throwsOnShortHash()
{
    TempRepo repo;
    bool threw = false;
    try { objectPath(repo.root, "ab"); }
    catch (const std::runtime_error &) { threw = true; }
    check("objectPath: throws on hash shorter than 4 characters", threw);
}

void test_objectPath_throwsOnEmptyHash()
{
    TempRepo repo;
    bool threw = false;
    try { objectPath(repo.root, ""); }
    catch (const std::runtime_error &) { threw = true; }
    check("objectPath: throws on empty hash", threw);
}

// exists 

void test_exists_returnsFalseForMissingObject()
{
    TempRepo repo;
    std::string hash = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    check("exists: returns false when object is not in store", !exists(repo.root, hash));
}

void test_exists_returnsTrueAfterStore()
{
    TempRepo repo;
    std::vector<unsigned char> contents = {'t', 'e', 's', 't'};
    auto filePath = repo.makeFile("input.bin", contents);

    std::string hash = storeFromFile(repo.root, filePath);
    check("exists: returns true after object is stored", exists(repo.root, hash));
}

// storeFromFile 

void test_storeFromFile_returnsCorrectHash()
{
    TempRepo repo;
    std::vector<unsigned char> contents = {'h', 'e', 'l', 'l', 'o'};
    auto filePath = repo.makeFile("hello.bin", contents);

    std::string storedHash = storeFromFile(repo.root, filePath);
    std::string expectedHash = hashFile(filePath);

    check("storeFromFile: returned hash matches hashFile output", storedHash == expectedHash);
}

void test_storeFromFile_objectAppearsOnDisk()
{
    TempRepo repo;
    std::vector<unsigned char> contents = {'s', 't', 'o', 'r', 'e'};
    auto filePath = repo.makeFile("store.bin", contents);

    std::string hash = storeFromFile(repo.root, filePath);
    std::filesystem::path expectedPath = objectPath(repo.root, hash);

    check("storeFromFile: object file exists at expected path",
          std::filesystem::exists(expectedPath));
}

void test_storeFromFile_deduplicatesIdenticalContent()
{
    TempRepo repo;
    std::vector<unsigned char> contents = {'d', 'u', 'p', 'e'};

    auto fileA = repo.makeFile("a.bin", contents);
    auto fileB = repo.makeFile("b.bin", contents);

    std::string hashA = storeFromFile(repo.root, fileA);
    std::string hashB = storeFromFile(repo.root, fileB);

    // Both should return the same hash and only one blob should exist on disk
    check("storeFromFile: identical content produces same hash", hashA == hashB);
    check("storeFromFile: identical content stored only once",
          std::filesystem::exists(objectPath(repo.root, hashA)));
}

void test_storeFromFile_differentContentDifferentHash()
{
    TempRepo repo;
    auto fileA = repo.makeFile("a.bin", {'a', 'a', 'a'});
    auto fileB = repo.makeFile("b.bin", {'b', 'b', 'b'});

    std::string hashA = storeFromFile(repo.root, fileA);
    std::string hashB = storeFromFile(repo.root, fileB);

    check("storeFromFile: different content produces different hashes", hashA != hashB);
}

void test_storeFromFile_preservesBinaryContent()
{
    TempRepo repo;
    // Null bytes, high bytes — representative of real audio file content
    std::vector<unsigned char> contents = {0x00, 0xFF, 0x80, 0x52, 0x49, 0x46, 0x46};
    auto filePath = repo.makeFile("binary.bin", contents);

    std::string hash = storeFromFile(repo.root, filePath);
    std::filesystem::path storedPath = objectPath(repo.root, hash);

    // Read stored blob directly and compare
    std::ifstream in(storedPath, std::ios::binary);
    std::vector<unsigned char> stored(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    check("storeFromFile: binary content is preserved exactly", stored == contents);
}

void test_storeFromFile_throwsOnMissingFile()
{
    TempRepo repo;
    bool threw = false;
    try { storeFromFile(repo.root, "/this/does/not/exist.bin"); }
    catch (...) { threw = true; }
    check("storeFromFile: throws when source file does not exist", threw);
}

void test_storeFromFile_createsShardedDirectory()
{
    TempRepo repo;
    auto filePath = repo.makeFile("shard.bin", {'s', 'h', 'a', 'r', 'd'});
    std::string hash = storeFromFile(repo.root, filePath);

    std::filesystem::path shardDir = repo.root / ".bvcs" / "objects" / hash.substr(0, 2);
    check("storeFromFile: sharded subdirectory is created", std::filesystem::is_directory(shardDir));
}

// retrieveToFile 

void test_retrieveToFile_roundTrip()
{
    TempRepo repo;
    std::vector<unsigned char> contents = {'r', 'o', 'u', 'n', 'd'};
    auto srcPath = repo.makeFile("src.bin", contents);
    std::string hash = storeFromFile(repo.root, srcPath);

    auto destPath = repo.root / "retrieved.bin";
    retrieveToFile(repo.root, hash, destPath);

    std::ifstream in(destPath, std::ios::binary);
    std::vector<unsigned char> result(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    check("retrieveToFile: retrieved content matches original", result == contents);
}

void test_retrieveToFile_preservesBinaryContent()
{
    TempRepo repo;
    std::vector<unsigned char> contents = {0x00, 0xFF, 0x7F, 0x80, 0x01, 0xFE};
    auto srcPath = repo.makeFile("binary_src.bin", contents);
    std::string hash = storeFromFile(repo.root, srcPath);

    auto destPath = repo.root / "binary_dest.bin";
    retrieveToFile(repo.root, hash, destPath);

    std::ifstream in(destPath, std::ios::binary);
    std::vector<unsigned char> result(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    check("retrieveToFile: binary content round-trip is lossless", result == contents);
}

void test_retrieveToFile_throwsOnMissingObject()
{
    TempRepo repo;
    std::string fakeHash = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    auto destPath = repo.root / "output.bin";

    bool threw = false;
    try { retrieveToFile(repo.root, fakeHash, destPath); }
    catch (const std::runtime_error &) { threw = true; }
    check("retrieveToFile: throws when hash does not exist in store", threw);
}

void test_retrieveToFile_overwritesExistingFile()
{
    TempRepo repo;
    std::vector<unsigned char> original = {'o', 'l', 'd'};
    std::vector<unsigned char> updated  = {'n', 'e', 'w'};

    auto srcPath = repo.makeFile("new.bin", updated);
    std::string hash = storeFromFile(repo.root, srcPath);

    // Write something else to the destination first
    auto destPath = repo.makeFile("dest.bin", original);
    retrieveToFile(repo.root, hash, destPath);

    std::ifstream in(destPath, std::ios::binary);
    std::vector<unsigned char> result(
        (std::istreambuf_iterator<char>(in)),
        std::istreambuf_iterator<char>()
    );
    check("retrieveToFile: overwrites existing file at destination", result == updated);
}

void test_retrieveToFile_noTempFileLeftBehind()
{
    TempRepo repo;
    auto srcPath = repo.makeFile("src.bin", {'t', 'e', 'm', 'p'});
    std::string hash = storeFromFile(repo.root, srcPath);

    auto destPath = repo.root / "out.bin";
    retrieveToFile(repo.root, hash, destPath);

    auto tempPath = destPath;
    tempPath += ".tmp";
    check("retrieveToFile: no .tmp file left behind", !std::filesystem::exists(tempPath));
}

// main 

int main()
{
    std::cout << "=== object_store tests ===\n\n";

    test_objectPath_correctStructure();
    test_objectPath_throwsOnShortHash();
    test_objectPath_throwsOnEmptyHash();

    test_exists_returnsFalseForMissingObject();
    test_exists_returnsTrueAfterStore();

    test_storeFromFile_returnsCorrectHash();
    test_storeFromFile_objectAppearsOnDisk();
    test_storeFromFile_deduplicatesIdenticalContent();
    test_storeFromFile_differentContentDifferentHash();
    test_storeFromFile_preservesBinaryContent();
    test_storeFromFile_throwsOnMissingFile();
    test_storeFromFile_createsShardedDirectory();

    test_retrieveToFile_roundTrip();
    test_retrieveToFile_preservesBinaryContent();
    test_retrieveToFile_throwsOnMissingObject();
    test_retrieveToFile_overwritesExistingFile();
    test_retrieveToFile_noTempFileLeftBehind();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}