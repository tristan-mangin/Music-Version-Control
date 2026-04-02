#include <iostream>
#include <filesystem>
#include <string>
#include "../storageHandling/index.h"

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

struct TempRepo
{
    std::filesystem::path root;

    TempRepo()
    {
        root = std::filesystem::temp_directory_path() / "bvcs_index_test_repo";
        std::filesystem::create_directories(root / ".storageHandling");
    }

    ~TempRepo()
    {
        std::filesystem::remove_all(root);
    }
};

const std::string VALID_HASH = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
const std::string OTHER_HASH = "9999999999999999999999999999999999999999999999999999999999999999";

// getStagedHash 

void test_getStagedHash_returnsEmptyWhenNoIndexFile()
{
    TempRepo repo;
    std::string result = getStagedHash(repo.root);
    check("getStagedHash: returns empty string when index file does not exist",
          result.empty());
}

void test_getStagedHash_returnsEmptyAfterClear()
{
    TempRepo repo;
    stage(repo.root, VALID_HASH);
    clear(repo.root);
    std::string result = getStagedHash(repo.root);
    check("getStagedHash: returns empty string after clear", result.empty());
}

// stage 

void test_stage_hashCanBeRetrieved()
{
    TempRepo repo;
    stage(repo.root, VALID_HASH);
    std::string result = getStagedHash(repo.root);
    check("stage: staged hash can be retrieved with getStagedHash",
          result == VALID_HASH);
}

void test_stage_overwritesPreviousHash()
{
    TempRepo repo;
    stage(repo.root, VALID_HASH);
    stage(repo.root, OTHER_HASH);
    std::string result = getStagedHash(repo.root);
    check("stage: staging a second hash overwrites the first",
          result == OTHER_HASH);
}

void test_stage_createsIndexFile()
{
    TempRepo repo;
    std::filesystem::path indexPath = repo.root / ".storageHandling" / "index";
    stage(repo.root, VALID_HASH);
    check("stage: index file exists after staging", std::filesystem::exists(indexPath));
}

void test_stage_hashIsPreservedExactly()
{
    TempRepo repo;
    stage(repo.root, VALID_HASH);
    std::string result = getStagedHash(repo.root);
    // No extra whitespace, no truncation
    check("stage: hash is stored and retrieved without modification",
          result == VALID_HASH && result.length() == VALID_HASH.length());
}

// clear

void test_clear_removesIndexFile()
{
    TempRepo repo;
    std::filesystem::path indexPath = repo.root / ".storageHandling" / "index";
    stage(repo.root, VALID_HASH);
    clear(repo.root);
    check("clear: index file is removed", !std::filesystem::exists(indexPath));
}

void test_clear_doesNothingWhenNoIndexFile()
{
    TempRepo repo;
    bool threw = false;
    try { clear(repo.root); }
    catch (...) { threw = true; }
    check("clear: does not throw when index file does not exist", !threw);
}

void test_clear_allowsRestageAfterClear()
{
    TempRepo repo;
    stage(repo.root, VALID_HASH);
    clear(repo.root);
    stage(repo.root, OTHER_HASH);
    std::string result = getStagedHash(repo.root);
    check("clear: can stage a new hash after clearing", result == OTHER_HASH);
}

// hasStaged 

void test_hasStaged_falseWhenNoIndexFile()
{
    TempRepo repo;
    check("hasStaged: returns false when nothing is staged", !hasStaged(repo.root));
}

void test_hasStaged_trueAfterStage()
{
    TempRepo repo;
    stage(repo.root, VALID_HASH);
    check("hasStaged: returns true after staging a hash", hasStaged(repo.root));
}

void test_hasStaged_falseAfterClear()
{
    TempRepo repo;
    stage(repo.root, VALID_HASH);
    clear(repo.root);
    check("hasStaged: returns false after clearing", !hasStaged(repo.root));
}

// round-trip 

void test_roundTrip_stageAndRetrieveMultipleTimes()
{
    TempRepo repo;
    // Simulate staging different versions of a file across multiple add operations
    stage(repo.root, VALID_HASH);
    check("round-trip: first hash correct", getStagedHash(repo.root) == VALID_HASH);

    stage(repo.root, OTHER_HASH);
    check("round-trip: second hash correct", getStagedHash(repo.root) == OTHER_HASH);

    clear(repo.root);
    check("round-trip: empty after clear", !hasStaged(repo.root));

    stage(repo.root, VALID_HASH);
    check("round-trip: can stage again after clear", getStagedHash(repo.root) == VALID_HASH);
}

// main 

int main()
{
    std::cout << "=== index tests ===\n\n";

    test_getStagedHash_returnsEmptyWhenNoIndexFile();
    test_getStagedHash_returnsEmptyAfterClear();

    test_stage_hashCanBeRetrieved();
    test_stage_overwritesPreviousHash();
    test_stage_createsIndexFile();
    test_stage_hashIsPreservedExactly();

    test_clear_removesIndexFile();
    test_clear_doesNothingWhenNoIndexFile();
    test_clear_allowsRestageAfterClear();

    test_hasStaged_falseWhenNoIndexFile();
    test_hasStaged_trueAfterStage();
    test_hasStaged_falseAfterClear();

    test_roundTrip_stageAndRetrieveMultipleTimes();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}