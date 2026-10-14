#ifndef CORE_LIBS_TESTS_FILESYSTEMTESTCLEANUP_HPP
#define CORE_LIBS_TESTS_FILESYSTEMTESTCLEANUP_HPP

#include <filesystem>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#include <chrono>
#include <string>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace TestFilesystem {

#ifdef _WIN32
namespace Detail {

inline bool Missing(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

[[noreturn]] inline void Fail(const std::filesystem::path& path, DWORD error) {
    throw std::filesystem::filesystem_error("test directory cleanup", path,
        std::error_code(static_cast<int>(error), std::system_category()));
}

struct SearchHandle {
    HANDLE value;
    ~SearchHandle() { FindClose(value); }
};

inline void Remove(const std::filesystem::path& path, std::chrono::steady_clock::time_point deadline) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const auto error = GetLastError();
        if (Missing(error)) return;
        Fail(path, error);
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        std::vector<std::filesystem::path> children;
        WIN32_FIND_DATAW entry{};
        const auto search = path / L"*";
        const HANDLE handle = FindFirstFileW(search.c_str(), &entry);
        if (handle != INVALID_HANDLE_VALUE) {
            SearchHandle owned{handle};
            do {
                const std::wstring name(entry.cFileName);
                if (name != L"." && name != L"..") children.push_back(path / name);
            } while (FindNextFileW(handle, &entry));
            const auto error = GetLastError();
            if (error != ERROR_NO_MORE_FILES && !Missing(error)) Fail(path, error);
        } else {
            const auto error = GetLastError();
            if (!Missing(error)) Fail(path, error);
        }
        for (const auto& child : children) Remove(child, deadline);
    }
    if ((attributes & FILE_ATTRIBUTE_READONLY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        if (!SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY)) {
            const auto error = GetLastError();
            if (Missing(error)) return;
            Fail(path, error);
        }
    }
    for (;;) {
        const BOOL removed = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? RemoveDirectoryW(path.c_str()) : DeleteFileW(path.c_str());
        if (removed) return;
        const auto error = GetLastError();
        if (Missing(error)) return;
        if (!(attributes & FILE_ATTRIBUTE_DIRECTORY) || error != ERROR_DIR_NOT_EMPTY || std::chrono::steady_clock::now() >= deadline) Fail(path, error);
        Sleep(1);
    }
}

}
#endif

inline void RemoveTree(const std::filesystem::path& path) {
    if (path.empty()) throw std::invalid_argument("test directory cleanup: empty path");
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    if (absolute == absolute.root_path() || absolute == std::filesystem::current_path().lexically_normal()) {
        throw std::invalid_argument("test directory cleanup: root or working directory");
    }
#ifdef _WIN32
    Detail::Remove(absolute, std::chrono::steady_clock::now() + std::chrono::milliseconds(500));
#else
    std::filesystem::remove_all(absolute);
#endif
}

}

#endif
