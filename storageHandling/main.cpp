// argument parsing and dispatching
#include <iostream>
#include <string>
#include <filesystem>
#include "repository.h"

// Forward declarations

void printUsage();
void printHelp(const std::string &command);
std::filesystem::path findRepoRoot();

// Command handlers

int handleInit(const std::filesystem::path &repoRoot)
{
    Repository repo(repoRoot);
    repo.init();
    return 0;
}

int handleAdd(const std::filesystem::path &repoRoot, const std::string &filePath)
{
    Repository repo(repoRoot);
    repo.add(filePath);
    return 0;
}

int handleCommit(const std::filesystem::path &repoRoot, const std::string &message)
{
    Repository repo(repoRoot);
    repo.commit(message);
    return 0;
}

int handleLog(const std::filesystem::path &repoRoot, bool jsonFormat)
{
    Repository repo(repoRoot);
    repo.log(jsonFormat);
    return 0;
}

int handleCheckout(const std::filesystem::path &repoRoot, const std::string &hashOrBranch, const std::filesystem::path &outputPath)
{
    Repository repo(repoRoot);
    repo.checkout(hashOrBranch, outputPath);
    return 0;
}

int handleStatus(const std::filesystem::path &repoRoot, const std::string &filePath, bool jsonFormat)
{
    Repository repo(repoRoot);
    repo.status(filePath, jsonFormat);
    return 0;
}

int main(int argc, char *argv[])
{
    // Require at least one argument (the command)
    if (argc < 2)
    {
        printUsage();
        return 1;
    }

    std::string command = argv[1];

    if (command == "help" || command == "--help" || command == "-h")
    {
        if (argc >= 3)
            printHelp(argv[2]);
        else
            printUsage();
        return 0;
    }

    if (command != "init" && command != "add" && command != "commit" &&
        command != "log" && command != "checkout" && command != "status")
    {
        std::cerr << "Unknown command: " << command << "\n";
        printUsage();
        return 1;
    }

    // init is special — it doesn't need an existing repo root
    if (command == "init")
    {
        return handleInit(std::filesystem::current_path());
    }

    // All other commands need to find the repo root first
    // This lets you run bvcs commands from subdirectories, just like git
    std::filesystem::path repoRoot;
    try
    {
        repoRoot = findRepoRoot();
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Wrap all command dispatch in a try/catch so any runtime error
    // from the repository layer prints cleanly rather than crashing
    try
    {
        if (command == "add")
        {
            if (argc < 3)
            {
                std::cerr << "Usage: bvcs add <file>\n";
                return 1;
            }
            return handleAdd(repoRoot, argv[2]);
        }
        else if (command == "commit")
        {
            // Support both: bvcs commit -m "message"
            //          and: bvcs commit "message"
            if (argc == 4 && std::string(argv[2]) == "-m")
            {
                return handleCommit(repoRoot, argv[3]);
            }
            else if (argc == 3)
            {
                return handleCommit(repoRoot, argv[2]);
            }
            else
            {
                std::cerr << "Usage: bvcs commit -m \"message\"\n";
                return 1;
            }
        }
        else if (command == "log")
        {
            bool jsonFormat = false;
            if (argc == 3)
            {
                if (std::string(argv[2]) == "--format=json")
                {
                    jsonFormat = true;
                }
                else
                {
                    std::cerr << "Usage: bvcs log [--format=json]\n";
                    return 1;
                }
            }
            else if (argc > 3)
            {
                std::cerr << "Usage: bvcs log [--format=json]\n";
                return 1;
            }

            return handleLog(repoRoot, jsonFormat);
        }
        else if (command == "checkout")
        {
            if (argc < 4)
            {
                std::cerr << "Usage: bvcs checkout <hash> <output_path>\n";
                return 1;
            }
            return handleCheckout(repoRoot, argv[2], argv[3]);
        }
        else if (command == "status")
        {
            if (argc < 3)
            {
                std::cerr << "Usage: bvcs status <file> [--format=json]\n";
                return 1;
            }

            bool jsonFormat = false;
            if (argc == 4)
            {
                if (std::string(argv[3]) == "--format=json")
                {
                    jsonFormat = true;
                }
                else
                {
                    std::cerr << "Usage: bvcs status <file> [--format=json]\n";
                    return 1;
                }
            }
            else if (argc > 4)
            {
                std::cerr << "Usage: bvcs status <file> [--format=json]\n";
                return 1;
            }

            return handleStatus(repoRoot, argv[2], jsonFormat);
        }
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

// Helpers

/**
 * Walks up the directory tree from the current working directory looking for a .bvcs folder
 * @throws std::runtime_error if no .bvcs folder is found in any parent directory
 * @return The path to the root of the repository (the directory containing .bvcs)
 */
std::filesystem::path findRepoRoot()
{
    std::filesystem::path current = std::filesystem::current_path();

    while (true)
    {
        if (std::filesystem::exists(current / ".bvcs"))
        {
            return current;
        }

        std::filesystem::path parent = current.parent_path();
        if (parent == current)
        {
            // Reached the filesystem root without finding a repo
            throw std::runtime_error(
                "Not a bvcs repository. Run 'bvcs init' first.");
        }
        current = parent;
    }
}

void printUsage()
{
    std::cout
        << "Usage: bvcs <command> [options]\n\n"
        << "Commands:\n"
        << "  init                      Initialize a new repository\n"
        << "  add <file>                Stage a file for committing\n"
        << "  commit -m <message>       Commit the staged file\n"
        << "  log [--format=json]       Show commit history\n"
        << "  checkout <hash> <output>  Restore a file from a commit\n"
        << "  status <file> [--format=json]  Show committed/staged/working hashes\n"
        << "  help <command>            Show help for a specific command\n";
}

void printHelp(const std::string &command)
{
    if (command == "init")
    {
        std::cout
            << "bvcs init\n\n"
            << "  Initializes a new bvcs repository in the current directory.\n"
            << "  Creates a .bvcs/ folder to store all version history.\n";
    }
    else if (command == "add")
    {
        std::cout
            << "bvcs add <file>\n\n"
            << "  Stages <file> for the next commit.\n"
            << "  The file's contents are hashed and stored in the object store.\n"
            << "  Only one file can be staged at a time.\n";
    }
    else if (command == "commit")
    {
        std::cout
            << "bvcs commit -m <message>\n\n"
            << "  Creates a new commit from the currently staged file.\n"
            << "  Records the file version, timestamp, and message.\n"
            << "  The staged file is cleared after a successful commit.\n";
    }
    else if (command == "log")
    {
        std::cout
            << "bvcs log [--format=json]\n\n"
            << "  Displays the commit history from newest to oldest.\n"
            << "  Each entry shows the commit hash, date, and message.\n"
            << "  Use --format=json to emit machine-readable output.\n";
    }
    else if (command == "checkout")
    {
        std::cout
            << "bvcs checkout <hash> <output_path>\n\n"
            << "  Retrieves the file associated with <hash> and writes\n"
            << "  it to <output_path>. The original file is not modified.\n"
            << "  Use 'bvcs log' to find commit hashes.\n";
    }
    else if (command == "status")
    {
        std::cout
            << "bvcs status <file> [--format=json]\n\n"
            << "  Compares three states for <file>:\n"
            << "  last committed blob hash, staged hash, and current working hash.\n"
            << "  Use --format=json to emit machine-readable output.\n";
    }
    else
    {
        std::cout << "Unknown command: " << command << "\n";
        printUsage();
    }
}