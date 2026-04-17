#include <iostream>
#include <filesystem>
#include <string>
#include <array>
#include <stdexcept>
#include <fstream>
#include <vector>

// ─── helpers ────────────────────────────────────────────────────────────────

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

// Runs a shell command and returns its exit code and stdout combined.
// cmd should be a complete shell command string.
struct CommandResult
{
    int exitCode;
    std::string output;
};

CommandResult run(const std::string &cmd)
{
    // Redirect stderr to stdout so we capture error messages too
    std::string fullCmd = cmd + " 2>&1";
    std::array<char, 256> buffer;
    std::string output;

#ifdef _WIN32
    FILE *pipe = _popen(fullCmd.c_str(), "r");
#else
    FILE *pipe = popen(fullCmd.c_str(), "r");
#endif

    if (!pipe)
    {
        throw std::runtime_error("Failed to run command: " + cmd);
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        output += buffer.data();
    }

#ifdef _WIN32
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif

    // pclose returns the raw wait status — extract the exit code
#ifndef _WIN32
    if (WIFEXITED(exitCode))
        exitCode = WEXITSTATUS(exitCode);
#endif

    return {exitCode, output};
}

// A self-cleaning temporary directory that also tracks the path to
// the bvcs binary so tests can invoke it without hardcoding a path.
struct TestEnv
{
    std::filesystem::path dir;
    std::string bvcsPath;

    // binaryPath should be the path to the compiled bvcs executable,
    // passed in from the test runner so it works regardless of build layout.
    TestEnv(const std::string &binaryPath)
        : bvcsPath(binaryPath)
    {
        dir = std::filesystem::temp_directory_path() / "bvcs_integration_test";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
    }

    ~TestEnv()
    {
        std::filesystem::remove_all(dir);
    }

    // Runs a bvcs command from inside the test directory
    CommandResult bvcs(const std::string &args)
    {
        return run("cd \"" + dir.string() + "\" && \"" + bvcsPath + "\" " + args);
    }

    // Writes a file inside the test directory
    std::filesystem::path makeFile(const std::string &name,
                                   const std::string &contents)
    {
        std::filesystem::path path = dir / name;
        std::ofstream out(path, std::ios::binary);
        out.write(contents.data(), contents.size());
        return path;
    }

    std::string readFile(const std::filesystem::path &path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    }
};

// ─── findRepoRoot (unit tested directly) ─────────────────────────────────────
// These tests call findRepoRoot() behaviour indirectly by observing whether
// commands succeed or fail from different working directories.

void test_findRepoRoot_findsRepoFromSubdirectory(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");

    // Create a subdirectory and run a command from inside it
    std::filesystem::create_directories(env.dir / "subdir");
    auto result = run("cd \"" + (env.dir / "subdir").string() +
                      "\" && \"" + binaryPath + "\" log");

    check("findRepoRoot: commands work from a subdirectory of the repo",
          result.exitCode == 0);
}

void test_findRepoRoot_failsOutsideRepo(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    // Don't init — run a command with no repo present
    auto result = env.bvcs("log");
    check("findRepoRoot: returns error when no repo exists",
          result.exitCode != 0);
}

// ─── no arguments ────────────────────────────────────────────────────────────

void test_noArgs_printsUsageAndFails(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    auto result = env.bvcs("");
    check("no args: exits with non-zero code", result.exitCode != 0);
    check("no args: prints usage information",
          result.output.find("Usage") != std::string::npos);
}

// ─── init ────────────────────────────────────────────────────────────────────

void test_init_succeedsInEmptyDirectory(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    auto result = env.bvcs("init");
    check("init: exits with code 0", result.exitCode == 0);
    check("init: .bvcs directory is created",
          std::filesystem::exists(env.dir / ".bvcs"));
}

void test_init_failsIfAlreadyInitialized(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    auto result = env.bvcs("init");
    check("init: exits with non-zero code if already initialized",
          result.exitCode != 0);
    check("init: error message mentions repository already exists",
          result.output.find("already") != std::string::npos ||
          result.output.find("Error") != std::string::npos);
}

// ─── add ─────────────────────────────────────────────────────────────────────

void test_add_succeedsWithValidFile(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    env.makeFile("test.wav", "audio content");
    auto result = env.bvcs("add test.wav");
    check("add: exits with code 0 for valid file", result.exitCode == 0);
}

void test_add_failsWithNoArguments(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    auto result = env.bvcs("add");
    check("add: exits with non-zero code when no file given",
          result.exitCode != 0);
}

void test_add_failsForMissingFile(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    auto result = env.bvcs("add nonexistent.wav");
    check("add: exits with non-zero code for missing file",
          result.exitCode != 0);
}

void test_add_failsOutsideRepo(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.makeFile("test.wav", "audio content");
    auto result = env.bvcs("add test.wav");
    check("add: exits with non-zero code outside a repo",
          result.exitCode != 0);
}

// ─── commit ──────────────────────────────────────────────────────────────────

void test_commit_succeedsWithDashM(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    env.makeFile("test.wav", "audio content");
    env.bvcs("add test.wav");
    auto result = env.bvcs("commit -m \"initial commit\"");
    check("commit: exits with code 0 using -m flag", result.exitCode == 0);
}

void test_commit_succeedsWithoutDashM(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    env.makeFile("test.wav", "audio content");
    env.bvcs("add test.wav");
    auto result = env.bvcs("commit \"initial commit\"");
    check("commit: exits with code 0 without -m flag", result.exitCode == 0);
}

void test_commit_failsWithNothingStaged(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    auto result = env.bvcs("commit -m \"nothing staged\"");
    check("commit: exits with non-zero code when nothing is staged",
          result.exitCode != 0);
}

void test_commit_failsWithNoMessage(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    env.makeFile("test.wav", "audio content");
    env.bvcs("add test.wav");
    auto result = env.bvcs("commit");
    check("commit: exits with non-zero code with no message",
          result.exitCode != 0);
}

void test_commit_failsWithEmptyMessage(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    env.makeFile("test.wav", "audio content");
    env.bvcs("add test.wav");
    auto result = env.bvcs("commit -m \"\"");
    check("commit: exits with non-zero code with empty message",
          result.exitCode != 0);
}

// ─── log ─────────────────────────────────────────────────────────────────────

void test_log_succeedsOnEmptyRepo(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    auto result = env.bvcs("log");
    check("log: exits with code 0 on repo with no commits",
          result.exitCode == 0);
}

void test_log_showsCommitAfterCommit(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    env.makeFile("test.wav", "audio content");
    env.bvcs("add test.wav");
    env.bvcs("commit -m \"first commit\"");
    auto result = env.bvcs("log");
    check("log: output contains commit message",
          result.output.find("first commit") != std::string::npos);
}

void test_log_showsMultipleCommits(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");

    env.makeFile("v1.wav", "version one");
    env.bvcs("add v1.wav");
    env.bvcs("commit -m \"version one\"");

    env.makeFile("v2.wav", "version two");
    env.bvcs("add v2.wav");
    env.bvcs("commit -m \"version two\"");

    auto result = env.bvcs("log");
    check("log: output contains both commit messages",
          result.output.find("version one") != std::string::npos &&
          result.output.find("version two") != std::string::npos);
}

// ─── checkout ────────────────────────────────────────────────────────────────

void test_checkout_restoresFile(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    env.makeFile("track.wav", "original audio");
    env.bvcs("add track.wav");
    env.bvcs("commit -m \"original\"");

    // Read HEAD to get the commit hash
    std::string hash = env.readFile(env.dir / ".bvcs" / "HEAD");

    auto result = env.bvcs("checkout " + hash + " restored.wav");
    check("checkout: exits with code 0", result.exitCode == 0);
    check("checkout: restored file contains original contents",
          env.readFile(env.dir / "restored.wav") == "original audio");
}

void test_checkout_failsWithUnknownHash(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    std::string fakeHash =
        "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
    auto result = env.bvcs("checkout " + fakeHash + " output.wav");
    check("checkout: exits with non-zero code for unknown hash",
          result.exitCode != 0);
}

void test_checkout_failsWithMissingArguments(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    env.bvcs("init");
    auto result = env.bvcs("checkout");
    check("checkout: exits with non-zero code when arguments are missing",
          result.exitCode != 0);
}

// ─── help ────────────────────────────────────────────────────────────────────

void test_help_printsUsage(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    auto result = env.bvcs("help");
    check("help: exits with code 0", result.exitCode == 0);
    check("help: output contains command list",
          result.output.find("init") != std::string::npos &&
          result.output.find("commit") != std::string::npos);
}

void test_help_commandSpecificHelp(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    auto result = env.bvcs("help commit");
    check("help commit: exits with code 0", result.exitCode == 0);
    check("help commit: output is specific to commit command",
          result.output.find("commit") != std::string::npos);
}

void test_unknownCommand_failsWithMessage(const std::string &binaryPath)
{
    TestEnv env(binaryPath);
    auto result = env.bvcs("notacommand");
    check("unknown command: exits with non-zero code", result.exitCode != 0);
    check("unknown command: output mentions unknown command",
          result.output.find("Unknown") != std::string::npos ||
          result.output.find("unknown") != std::string::npos);
}

// ─── main ────────────────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    // The path to the bvcs binary is passed as the first argument so
    // this test file works regardless of where CMake puts the binary.
    // In CMakeLists.txt this is handled by passing $<TARGET_FILE:bvcs>
    if (argc < 2)
    {
        std::cerr << "Usage: test_main <path_to_bvcs_binary>\n";
        return 1;
    }

    std::string binaryPath = argv[1];

    if (!std::filesystem::exists(binaryPath))
    {
        std::cerr << "Binary not found: " << binaryPath << "\n";
        return 1;
    }

    std::cout << "=== integration tests (main) ===\n\n";

    test_noArgs_printsUsageAndFails(binaryPath);

    test_findRepoRoot_findsRepoFromSubdirectory(binaryPath);
    test_findRepoRoot_failsOutsideRepo(binaryPath);

    test_init_succeedsInEmptyDirectory(binaryPath);
    test_init_failsIfAlreadyInitialized(binaryPath);

    test_add_succeedsWithValidFile(binaryPath);
    test_add_failsWithNoArguments(binaryPath);
    test_add_failsForMissingFile(binaryPath);
    test_add_failsOutsideRepo(binaryPath);

    test_commit_succeedsWithDashM(binaryPath);
    test_commit_succeedsWithoutDashM(binaryPath);
    test_commit_failsWithNothingStaged(binaryPath);
    test_commit_failsWithNoMessage(binaryPath);
    test_commit_failsWithEmptyMessage(binaryPath);

    test_log_succeedsOnEmptyRepo(binaryPath);
    test_log_showsCommitAfterCommit(binaryPath);
    test_log_showsMultipleCommits(binaryPath);

    test_checkout_restoresFile(binaryPath);
    test_checkout_failsWithUnknownHash(binaryPath);
    test_checkout_failsWithMissingArguments(binaryPath);

    test_help_printsUsage(binaryPath);
    test_help_commandSpecificHelp(binaryPath);
    test_unknownCommand_failsWithMessage(binaryPath);

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}