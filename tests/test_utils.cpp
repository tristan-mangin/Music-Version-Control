#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <ctime>
#include <string>
#include "../storageHandling/utils.h"

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

std::filesystem::path makeTempPath(const std::string &filename)
{
    return std::filesystem::temp_directory_path() / filename;
}

void writeBytesToFile(const std::filesystem::path &path, const std::vector<unsigned char> &data)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(data.data()), data.size());
}

// readFileBinary

void test_readFileBinary_correctContents()
{
    std::vector<unsigned char> expected = {'h', 'e', 'l', 'l', 'o'};
    auto path = makeTempPath("bvcs_read_test.bin");
    writeBytesToFile(path, expected);

    auto result = readFileBinary(path);
    std::filesystem::remove(path);

    check("readFileBinary: returns correct contents", result == expected);
}

void test_readFileBinary_preservesBinaryData()
{
    // Null bytes and high bytes are common in audio files — make sure they survive
    std::vector<unsigned char> expected = {0x00, 0xFF, 0x80, 0x01, 0xFE};
    auto path = makeTempPath("bvcs_binary_test.bin");
    writeBytesToFile(path, expected);

    auto result = readFileBinary(path);
    std::filesystem::remove(path);

    check("readFileBinary: preserves binary data including null and high bytes", result == expected);
}

void test_readFileBinary_emptyFile()
{
    auto path = makeTempPath("bvcs_empty_test.bin");
    writeBytesToFile(path, {});

    auto result = readFileBinary(path);
    std::filesystem::remove(path);

    check("readFileBinary: empty file returns empty vector", result.empty());
}

void test_readFileBinary_throwsOnMissingFile()
{
    bool threw = false;
    try
    {
        readFileBinary("/this/path/does/not/exist.bin");
    }
    catch (const std::runtime_error &)
    {
        threw = true;
    }
    check("readFileBinary: throws on missing file", threw);
}

// writeFileAtomic

void test_writeFileAtomic_writesCorrectContents()
{
    std::vector<unsigned char> data = {'w', 'r', 'i', 't', 'e'};
    auto path = makeTempPath("bvcs_write_test.bin");

    writeFileAtomic(path, data);
    auto result = readFileBinary(path);
    std::filesystem::remove(path);

    check("writeFileAtomic: written contents match input", result == data);
}

void test_writeFileAtomic_noTempFileLeftBehind()
{
    std::vector<unsigned char> data = {'t', 'e', 'm', 'p'};
    auto path = makeTempPath("bvcs_atomic_test.bin");
    auto tempPath = path;
    tempPath += ".tmp";

    writeFileAtomic(path, data);

    bool tempExists = std::filesystem::exists(tempPath);
    std::filesystem::remove(path);

    check("writeFileAtomic: no .tmp file left behind after write", !tempExists);
}

void test_writeFileAtomic_overwritesExistingFile()
{
    auto path = makeTempPath("bvcs_overwrite_test.bin");
    std::vector<unsigned char> original = {'o', 'l', 'd'};
    std::vector<unsigned char> updated = {'n', 'e', 'w'};

    writeFileAtomic(path, original);
    writeFileAtomic(path, updated);
    auto result = readFileBinary(path);
    std::filesystem::remove(path);

    check("writeFileAtomic: overwrites existing file correctly", result == updated);
}

void test_writeFileAtomic_roundTrip()
{
    // Binary data with null bytes — representative of real audio file content
    std::vector<unsigned char> data = {0x52, 0x49, 0x46, 0x46, 0x00, 0xFF, 0x80};
    auto path = makeTempPath("bvcs_roundtrip_test.bin");

    writeFileAtomic(path, data);
    auto result = readFileBinary(path);
    std::filesystem::remove(path);

    check("writeFileAtomic + readFileBinary round-trip preserves binary data", result == data);
}

// formatTimestamp

void test_formatTimestamp_knownValue()
{
    // 2024-01-15 12:30:45 UTC — construct this as a known local time
    std::tm tm{};
    tm.tm_year = 124; // years since 1900
    tm.tm_mon = 0;    // January (0-indexed)
    tm.tm_mday = 15;
    tm.tm_hour = 12;
    tm.tm_min = 30;
    tm.tm_sec = 45;
    tm.tm_isdst = -1;
    std::time_t timestamp = std::mktime(&tm);

    std::string result = formatTimestamp(timestamp);

    check("formatTimestamp: produces correct format for known value",
          result == "2024-01-15 12:30:45");
}

void test_formatTimestamp_correctLength()
{
    std::time_t now = std::time(nullptr);
    std::string result = formatTimestamp(now);
    // "YYYY-MM-DD HH:MM:SS" is always exactly 19 characters
    check("formatTimestamp: result is always 19 characters", result.length() == 19);
}

void test_formatTimestamp_correctStructure()
{
    std::time_t now = std::time(nullptr);
    std::string result = formatTimestamp(now);
    // Check the separators are in the right positions
    bool structureCorrect = result[4] == '-' && result[7] == '-' &&
                            result[10] == ' ' && result[13] == ':' &&
                            result[16] == ':';
    check("formatTimestamp: separators are in correct positions", structureCorrect);
}

// hexToBytes

void test_hexToBytes_knownValue()
{
    // "hello" in hex is 68 65 6c 6c 6f
    std::vector<unsigned char> expected = {0x68, 0x65, 0x6c, 0x6c, 0x6f};
    auto result = hexToBytes("68656c6c6f");
    check("hexToBytes: converts known hex string correctly", result == expected);
}

void test_hexToBytes_uppercaseHex()
{
    std::vector<unsigned char> expected = {0xAB, 0xCD};
    auto result = hexToBytes("ABCD");
    check("hexToBytes: handles uppercase hex characters", result == expected);
}

void test_hexToBytes_emptyString()
{
    auto result = hexToBytes("");
    check("hexToBytes: empty string returns empty vector", result.empty());
}

void test_hexToBytes_throwsOnOddLength()
{
    bool threw = false;
    try
    {
        hexToBytes("abc");
    }
    catch (const std::runtime_error &)
    {
        threw = true;
    }
    check("hexToBytes: throws on odd-length string", threw);
}

void test_hexToBytes_throwsOnInvalidCharacters()
{
    bool threw = false;
    try
    {
        hexToBytes("zz");
    }
    catch (const std::runtime_error &)
    {
        threw = true;
    }
    check("hexToBytes: throws on non-hex characters", threw);
}

void test_hexToBytes_roundTripWithToHexString()
{
    // toHexString then hexToBytes should return the original bytes
    unsigned char original[] = {0x2c, 0xf2, 0x4d, 0xba, 0x5f};
    std::string hex = toHexString(original, 5);
    auto result = hexToBytes(hex);

    std::vector<unsigned char> expected(original, original + 5);
    check("hexToBytes + toHexString round-trip is lossless", result == expected);
}

// toHexString

void test_toHexString_knownValue()
{
    unsigned char input[] = {0x68, 0x65, 0x6c, 0x6c, 0x6f};
    std::string result = toHexString(input, 5);
    check("toHexString: produces correct hex for known bytes", result == "68656c6c6f");
}

void test_toHexString_zeroBytesPadded()
{
    // Single zero byte should produce "00", not "0"
    unsigned char input[] = {0x00};
    std::string result = toHexString(input, 1);
    check("toHexString: zero byte is padded to two characters", result == "00");
}

void test_toHexString_allFF()
{
    unsigned char input[] = {0xFF, 0xFF};
    std::string result = toHexString(input, 2);
    check("toHexString: 0xFF bytes produce 'ffff'", result == "ffff");
}

int main()
{
    std::cout << "=== utils tests ===\n\n";

    test_readFileBinary_correctContents();
    test_readFileBinary_preservesBinaryData();
    test_readFileBinary_emptyFile();
    test_readFileBinary_throwsOnMissingFile();

    test_writeFileAtomic_writesCorrectContents();
    test_writeFileAtomic_noTempFileLeftBehind();
    test_writeFileAtomic_overwritesExistingFile();
    test_writeFileAtomic_roundTrip();

    test_formatTimestamp_knownValue();
    test_formatTimestamp_correctLength();
    test_formatTimestamp_correctStructure();

    test_hexToBytes_knownValue();
    test_hexToBytes_uppercaseHex();
    test_hexToBytes_emptyString();
    test_hexToBytes_throwsOnOddLength();
    test_hexToBytes_throwsOnInvalidCharacters();
    test_hexToBytes_roundTripWithToHexString();

    test_toHexString_knownValue();
    test_toHexString_zeroBytesPadded();
    test_toHexString_allFF();

    std::cout << "\n"
              << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}