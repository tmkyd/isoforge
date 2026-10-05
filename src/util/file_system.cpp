// SPDX-License-Identifier: Apache-2.0

#include "util/file_system.h"

#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <vector>

#include "util/unique_handle.h"

namespace isoforge::util {
namespace {

std::wstring reason(std::uint32_t error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
        return L"file not found";
    case ERROR_PATH_NOT_FOUND:
        return L"directory not found";
    case ERROR_ACCESS_DENIED:
        return L"access denied";
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return L"the file is in use";
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return L"the file already exists";
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return L"not enough space";
    case ERROR_FILE_TOO_LARGE:
        return L"the file is too large for the file system";
    case ERROR_WRITE_PROTECT:
        return L"the media is write protected";
    default:
        return L"Windows error " + std::to_wstring(error);
    }
}

std::wstring upper(std::wstring_view text) {
    std::wstring result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
    return result;
}

class Win32WritableFile final : public WritableFile {
public:
    Win32WritableFile(UniqueHandle handle, std::wstring path) : handle_(std::move(handle)), path_(std::move(path)) {}

    void write(std::span<const std::uint8_t> data) override {
        while (!data.empty()) {
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size(), 1u << 30));
            DWORD written = 0;
            if (!::WriteFile(handle_.get(), data.data(), chunk, &written, nullptr) || written == 0) {
                throw FileError::from_win32(::GetLastError(), L"cannot write", path_);
            }
            data = data.subspan(written);
        }
    }

    void finish() override {
        if (!::FlushFileBuffers(handle_.get())) {
            throw FileError::from_win32(::GetLastError(), L"cannot flush", path_);
        }
        // Close explicitly so that a failure is reported.
        if (!::CloseHandle(handle_.release())) {
            throw FileError::from_win32(::GetLastError(), L"cannot close", path_);
        }
    }

private:
    UniqueHandle handle_;
    std::wstring path_;
};

class Win32ReadableFile final : public ReadableFile {
public:
    Win32ReadableFile(UniqueHandle handle, std::wstring path) : handle_(std::move(handle)), path_(std::move(path)) {}

    std::size_t read(std::span<std::uint8_t> buffer) override {
        DWORD got = 0;
        const DWORD want = static_cast<DWORD>(std::min<std::size_t>(buffer.size(), 1u << 30));
        if (!::ReadFile(handle_.get(), buffer.data(), want, &got, nullptr)) {
            throw FileError::from_win32(::GetLastError(), L"cannot read", path_);
        }
        return got;
    }

private:
    UniqueHandle handle_;
    std::wstring path_;
};

}  // namespace

FileError::FileError(std::wstring message) : std::runtime_error("file error"), message_(std::move(message)) {}

FileError FileError::from_win32(std::uint32_t win32_error, std::wstring_view action, std::wstring_view path) {
    return FileError(std::wstring(action) + L" '" + std::wstring(path) + L"': " + reason(win32_error));
}

std::optional<std::uint64_t> max_file_size_for(std::wstring_view filesystem) {
    const std::wstring name = upper(filesystem);
    if (name == L"FAT32") {
        return 0xFFFF'FFFFull;  // 4 GiB - 1.
    }
    if (name == L"FAT" || name == L"FAT12" || name == L"FAT16") {
        return 0x7FFF'FFFFull;  // 2 GiB - 1.
    }
    return std::nullopt;
}

std::wstring parent_directory(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    // Keep the separator of a root such as "C:\".
    if (slash == 2 && path.size() > 2 && path[1] == L':') {
        return path.substr(0, 3);
    }
    return path.substr(0, slash);
}

std::wstring file_name(const std::wstring& path) {
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring Win32FileSystem::full_path(const std::wstring& path) {
    const DWORD length = ::GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (length == 0) {
        throw FileError::from_win32(::GetLastError(), L"invalid path", path);
    }
    std::wstring full(length, L'\0');
    const DWORD written = ::GetFullPathNameW(path.c_str(), length, full.data(), nullptr);
    if (written == 0 || written >= length) {
        throw FileError::from_win32(::GetLastError(), L"invalid path", path);
    }
    full.resize(written);
    return full;
}

bool Win32FileSystem::exists(const std::wstring& path) {
    return ::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool Win32FileSystem::directory_exists(const std::wstring& path) {
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::optional<std::uint64_t> Win32FileSystem::file_size(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) ||
        (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return std::nullopt;
    }
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

VolumeInfo Win32FileSystem::volume_info(const std::wstring& path) {
    std::vector<wchar_t> root(path.size() + 2);
    if (!::GetVolumePathNameW(path.c_str(), root.data(), static_cast<DWORD>(root.size()))) {
        throw FileError::from_win32(::GetLastError(), L"cannot find the volume of", path);
    }
    ULARGE_INTEGER available{};
    if (!::GetDiskFreeSpaceExW(root.data(), &available, nullptr, nullptr)) {
        throw FileError::from_win32(::GetLastError(), L"cannot get the free space of", root.data());
    }
    wchar_t filesystem[MAX_PATH + 1] = {};
    if (!::GetVolumeInformationW(root.data(), nullptr, 0, nullptr, nullptr, nullptr, filesystem, MAX_PATH + 1)) {
        throw FileError::from_win32(::GetLastError(), L"cannot get the file system of", root.data());
    }
    return VolumeInfo{available.QuadPart, filesystem, max_file_size_for(filesystem)};
}

std::unique_ptr<WritableFile> Win32FileSystem::create(const std::wstring& path, CreateMode mode) {
    UniqueHandle handle(::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                      mode == CreateMode::Replace ? CREATE_ALWAYS : OPEN_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!handle.valid()) {
        throw FileError::from_win32(::GetLastError(), L"cannot create", path);
    }
    if (mode == CreateMode::Append) {
        LARGE_INTEGER zero{};
        if (!::SetFilePointerEx(handle.get(), zero, nullptr, FILE_END)) {
            throw FileError::from_win32(::GetLastError(), L"cannot append to", path);
        }
    }
    return std::make_unique<Win32WritableFile>(std::move(handle), path);
}

std::unique_ptr<ReadableFile> Win32FileSystem::open(const std::wstring& path) {
    UniqueHandle handle(::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    if (!handle.valid()) {
        throw FileError::from_win32(::GetLastError(), L"cannot open", path);
    }
    return std::make_unique<Win32ReadableFile>(std::move(handle), path);
}

void Win32FileSystem::rename(const std::wstring& from, const std::wstring& to, bool replace) {
    const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
    if (!::MoveFileExW(from.c_str(), to.c_str(), flags)) {
        throw FileError::from_win32(::GetLastError(), L"cannot rename to", to);
    }
}

bool Win32FileSystem::remove(const std::wstring& path) noexcept {
    return ::DeleteFileW(path.c_str()) != 0;
}

}  // namespace isoforge::util
