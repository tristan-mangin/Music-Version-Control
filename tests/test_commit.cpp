#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include "../storageHandling/commit.h"

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

// Builds a commit with predictable values for use across multiple tests
Commit makeTestCommit()
{
    Commit c;
    c.hash       = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    c.parentHash = "1111111111111111111111111111111111111111111111111111111111111111";
    c.blobHash   = "2222222222222222222222222222222222222222222222222222222222222222";
    c.message    = "initial commit";
    c.timestamp  = 1705316400; // 2024-01-15 09:00:00 UTC — fixed value for determinism
    return c;
}

// serializeCommit 

void test_serialize_returnsNonEmptyData()
{
    Commit c = makeTestCommit();
    auto data = serializeCommit(c);
    check("serializeCommit: returns non-empty byte vector", !data.empty());
}

void test_serialize_containsParentHash()
{
    Commit c = makeTestCommit();
    auto data = serializeCommit(c);
    std::string str(data.begin(), data.end());
    check("serializeCommit: output contains parentHash",
          str.find(c.parentHash) != std::string::npos);
}

void test_serialize_containsBlobHash()
{
    Commit c = makeTestCommit();
    auto data = serializeCommit(c);
    std::string str(data.begin(), data.end());
    check("serializeCommit: output contains blobHash",
          str.find(c.blobHash) != std::string::npos);
}

void test_serialize_containsMessage()
{
    Commit c = makeTestCommit();
    auto data = serializeCommit(c);
    std::string str(data.begin(), data.end());
    check("serializeCommit: output contains message",
          str.find(c.message) != std::string::npos);
}

void test_serialize_doesNotContainCommitHash()
{
    // The commit's own hash is computed from the serialized content,
    // so it must not be part of that content — that would be circular
    Commit c = makeTestCommit();
    auto data = serializeCommit(c);
    std::string str(data.begin(), data.end());
    check("serializeCommit: output does not contain the commit's own hash",
          str.find(c.hash) == std::string::npos);
}

void test_serialize_deterministicOutput()
{
    Commit c = makeTestCommit();
    auto first  = serializeCommit(c);
    auto second = serializeCommit(c);
    check("serializeCommit: same commit produces identical output each time",
          first == second);
}

void test_serialize_differentCommitsDifferentOutput()
{
    Commit a = makeTestCommit();
    Commit b = makeTestCommit();
    b.message = "a different message";

    auto dataA = serializeCommit(a);
    auto dataB = serializeCommit(b);
    check("serializeCommit: different commits produce different output",
          dataA != dataB);
}

void test_serialize_emptyParentHashForRootCommit()
{
    // The first commit in a repo has no parent — empty string must serialize cleanly
    Commit c = makeTestCommit();
    c.parentHash = "";
    bool threw = false;
    try { serializeCommit(c); }
    catch (...) { threw = true; }
    check("serializeCommit: handles empty parentHash without throwing", !threw);
}

// deserializeCommit 

void test_deserialize_roundTripParentHash()
{
    Commit original = makeTestCommit();
    auto data = serializeCommit(original);
    Commit restored = deserializeCommit(data);
    check("deserializeCommit: parentHash survives round-trip",
          restored.parentHash == original.parentHash);
}

void test_deserialize_roundTripBlobHash()
{
    Commit original = makeTestCommit();
    auto data = serializeCommit(original);
    Commit restored = deserializeCommit(data);
    check("deserializeCommit: blobHash survives round-trip",
          restored.blobHash == original.blobHash);
}

void test_deserialize_roundTripMessage()
{
    Commit original = makeTestCommit();
    auto data = serializeCommit(original);
    Commit restored = deserializeCommit(data);
    check("deserializeCommit: message survives round-trip",
          restored.message == original.message);
}

void test_deserialize_roundTripTimestamp()
{
    Commit original = makeTestCommit();
    auto data = serializeCommit(original);
    Commit restored = deserializeCommit(data);
    check("deserializeCommit: timestamp survives round-trip",
          restored.timestamp == original.timestamp);
}

void test_deserialize_hashIsEmptyAfterDeserialize()
{
    // The hash field is not serialized — it must be populated by the caller
    // after deserializing, by hashing the raw data itself
    Commit original = makeTestCommit();
    auto data = serializeCommit(original);
    Commit restored = deserializeCommit(data);
    check("deserializeCommit: hash field is empty after deserialization",
          restored.hash.empty());
}

void test_deserialize_emptyParentHashRoundTrip()
{
    Commit original = makeTestCommit();
    original.parentHash = "";
    auto data = serializeCommit(original);
    Commit restored = deserializeCommit(data);
    check("deserializeCommit: empty parentHash round-trips correctly",
          restored.parentHash.empty());
}

void test_deserialize_throwsOnEmptyData()
{
    bool threw = false;
    try { deserializeCommit({}); }
    catch (const std::runtime_error &) { threw = true; }
    check("deserializeCommit: throws on empty input", threw);
}

void test_deserialize_throwsOnCorruptData()
{
    std::vector<unsigned char> garbage = {'x', 'x', 'x'};
    bool threw = false;
    try { deserializeCommit(garbage); }
    catch (const std::runtime_error &) { threw = true; }
    check("deserializeCommit: throws on corrupt data", threw);
}

void test_deserialize_messageWithSpecialCharacters()
{
    Commit original = makeTestCommit();
    original.message = "fix: handle edge case with 'quotes' and symbols !@#$%";
    auto data = serializeCommit(original);
    Commit restored = deserializeCommit(data);
    check("deserializeCommit: message with special characters round-trips correctly",
          restored.message == original.message);
}

// main

int main()
{
    std::cout << "=== commit tests ===\n\n";

    test_serialize_returnsNonEmptyData();
    test_serialize_containsParentHash();
    test_serialize_containsBlobHash();
    test_serialize_containsMessage();
    test_serialize_doesNotContainCommitHash();
    test_serialize_deterministicOutput();
    test_serialize_differentCommitsDifferentOutput();
    test_serialize_emptyParentHashForRootCommit();

    test_deserialize_roundTripParentHash();
    test_deserialize_roundTripBlobHash();
    test_deserialize_roundTripMessage();
    test_deserialize_roundTripTimestamp();
    test_deserialize_hashIsEmptyAfterDeserialize();
    test_deserialize_emptyParentHashRoundTrip();
    test_deserialize_throwsOnEmptyData();
    test_deserialize_throwsOnCorruptData();
    test_deserialize_messageWithSpecialCharacters();

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}