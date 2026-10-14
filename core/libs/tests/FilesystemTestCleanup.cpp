#include "FilesystemTestCleanup.hpp"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <fstream>
#include <iostream>
#include <thread>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "cleanup check failed at line %u\n", location.line());
        std::abort();
    }
}

static bool Refused(const std::filesystem::path& path) {
    try { TestFilesystem::RemoveTree(path); }
    catch (const std::invalid_argument&) { return true; }
    return false;
}

int main() {
    const auto base = std::filesystem::current_path() / ("anyps5-cleanup-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(base.parent_path() == std::filesystem::current_path());
    Require(Refused({}) && Refused(std::filesystem::current_path().root_path()) && Refused(std::filesystem::current_path()));
    Require(std::filesystem::create_directories(base / "tree" / "nested"));
    { std::ofstream file(base / "tree" / "nested" / "file"); file << "fixture"; Require(file.good()); }
    TestFilesystem::RemoveTree(base / "absent");
    TestFilesystem::RemoveTree(base / "tree");
    TestFilesystem::RemoveTree(base / "tree");
    Require(!std::filesystem::exists(base / "tree"));
    const auto outside = base / "preserved";
    Require(std::filesystem::create_directory(outside));
    { std::ofstream file(outside / "file"); file << "preserved"; }
    const auto links = base / "links";
    Require(std::filesystem::create_directory(links));
#ifdef _WIN32
    Require(CreateSymbolicLinkW((links / "directory").c_str(), outside.c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | 2) != 0);
    Require(CreateSymbolicLinkW((links / "dangling").c_str(), L"missing", 2) != 0);
    const auto readonly = links / "readonly";
    { std::ofstream file(readonly); file << "fixture"; }
    Require(SetFileAttributesW(readonly.c_str(), FILE_ATTRIBUTE_READONLY) != 0);
#else
    std::filesystem::create_directory_symlink(outside, links / "directory");
    std::filesystem::create_symlink("missing", links / "dangling");
#endif
    TestFilesystem::RemoveTree(links);
    Require(std::filesystem::is_regular_file(outside / "file"));
#ifdef _WIN32
    const auto pending = base / "pending";
    const auto child = pending / "child";
    Require(std::filesystem::create_directories(child));
    const HANDLE handle = CreateFileW(child.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    Require(handle != INVALID_HANDLE_VALUE);
    Require(RemoveDirectoryW(child.c_str()) != 0);
    std::jthread release([handle] { Sleep(20); CloseHandle(handle); });
    TestFilesystem::RemoveTree(pending);
    Require(!std::filesystem::exists(pending));
    release.join();
    const auto blocked = base / "blocked";
    Require(std::filesystem::create_directory(blocked));
    const auto locked = blocked / "locked";
    { std::ofstream file(locked); file << "fixture"; }
    const HANDLE lockedHandle = CreateFileW(locked.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Require(lockedHandle != INVALID_HANDLE_VALUE);
    bool failed = false;
    try { TestFilesystem::RemoveTree(blocked); }
    catch (const std::filesystem::filesystem_error& error) { failed = error.code().value() == ERROR_SHARING_VIOLATION; }
    CloseHandle(lockedHandle);
    Require(failed && std::filesystem::exists(locked));
#endif
    TestFilesystem::RemoveTree(base);
    Require(!std::filesystem::exists(base));
    std::cout << "test fixture cleanup checks passed\n";
}
