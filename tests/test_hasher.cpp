#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include "../storageHandling/hasher.h"

// HELPERS

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

// Creates a temporary file with the given contents and returns its path
std::filesystem::path makeTempFile(const std::string &contents)
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / "bvcs_test_temp.bin";
    std::ofstream out(path, std::ios::binary);
    out.write(contents.data(), contents.size());
    return path;
}

// TESTS

// SHA-256 of the empty string is a well-known value, easy to verify externally
// with: echo -n "" | sha256sum
void test_hashBytes_knownValue()
{
    const unsigned char empty[] = {};
    std::string result = hashBytes(empty, 0);
    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    check("hashBytes: empty input matches known SHA-256", result == expected);
}

// SHA-256 of "hello" is also well known
// verify with: echo -n "hello" | sha256sum
void test_hashBytes_helloWorld()
{
    const unsigned char input[] = {'h', 'e', 'l', 'l', 'o'};
    std::string result = hashBytes(input, 5);
    std::string expected = "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824";
    check("hashBytes: 'hello' matches known SHA-256", result == expected);
}

// hashFile and hashBytes should agree when given the same data
void test_hashFile_matchesHashBytes()
{
    std::string contents = "some binary-ish content \x01\x02\x03";
    std::filesystem::path tmp = makeTempFile(contents);

    std::string fromFile  = hashFile(tmp);
    std::string fromBytes = hashBytes(
        reinterpret_cast<const unsigned char *>(contents.data()),
        contents.size()
    );

    std::filesystem::remove(tmp);
    check("hashFile and hashBytes agree on same data", fromFile == fromBytes);
}

// Hashing the same file twice should give the same result
void test_hashFile_isDeterministic()
{
    std::filesystem::path tmp = makeTempFile("determinism check");
    std::string first  = hashFile(tmp);
    std::string second = hashFile(tmp);
    std::filesystem::remove(tmp);
    check("hashFile is deterministic", first == second);
}

// Different contents must produce different hashes
void test_hashBytes_differentInputsDifferentHashes()
{
    const unsigned char a[] = {'a'};
    const unsigned char b[] = {'b'};
    check("hashBytes: different inputs produce different hashes",
          hashBytes(a, 1) != hashBytes(b, 1));
}

// hashFile should throw when the file doesn't exist
void test_hashFile_throwsOnMissingFile()
{
    bool threw = false;
    try
    {
        std::filesystem::path missingFile("/this/path/does/not/exist.bin");
        hashFile(missingFile);
    }
    catch (const std::runtime_error &)
    {
        threw = true;
    }
    check("hashFile throws on missing file", threw);
}

int main()
{
    std::cout << "=== hasher tests ===\n\n";

    test_hashBytes_knownValue();
    test_hashBytes_helloWorld();
    test_hashFile_matchesHashBytes();
    test_hashFile_isDeterministic();
    test_hashBytes_differentInputsDifferentHashes();
    test_hashFile_throwsOnMissingFile();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}