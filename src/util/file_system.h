// SPDX-License-Identifier: Apache-2.0
//
// File operations used by create and verify, behind an interface so that tests can use an
// in-memory implementation. These are ordinary files; the optical drive is never read through
// this interface.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace isoforge::util {

// A failed file operation. message() is user-facing and may contain non-ASCII paths.
class FileError : public std::runtime_error {
public:
    explicit FileError(std::wstring message);

    const std::wstring& message() const noexcept { return message_; }

    // e.g. FileError::from_win32(5, L"cannot create", path) -> "cannot create 'path': access denied".
    static FileError from_win32(std::uint32_t win32_error, std::wstring_view action, std::wstring_view path);

private:
    std::wstring message_;
};

struct VolumeInfo {
    std::uint64_t free_bytes = 0;               // Available to the caller.
    std::wstring filesystem;                    // e.g. L"NTFS", L"FAT32", L"exFAT".
    std::optional<std::uint64_t> max_file_size;  // Largest file the file system can store, if limited.
};

// Largest file size for a file system name: FAT32 4 GiB - 1, FAT 2 GiB - 1, others unlimited.
std::optional<std::uint64_t> max_file_size_for(std::wstring_view filesystem);

class WritableFile {
public:
    virtual ~WritableFile() = default;  // Closes the file without flushing if finish() was not called.
    virtual void write(std::span<const std::uint8_t> data) = 0;
    // Flushes and closes the file.
    virtual void finish() = 0;
};

class ReadableFile {
public:
    virtual ~ReadableFile() = default;
    // Returns the number of bytes read; 0 at the end of the file.
    virtual std::size_t read(std::span<std::uint8_t> buffer) = 0;
};

enum class CreateMode {
    Replace,  // Create or truncate.
    Append,   // Create or append to the end.
};

// All operations throw FileError except remove().
class FileSystem {
public:
    virtual ~FileSystem() = default;
    virtual std::wstring full_path(const std::wstring& path) = 0;
    virtual bool exists(const std::wstring& path) = 0;  // File or directory.
    virtual bool directory_exists(const std::wstring& path) = 0;
    // Size of an existing file, or std::nullopt if there is no such file.
    virtual std::optional<std::uint64_t> file_size(const std::wstring& path) = 0;
    virtual VolumeInfo volume_info(const std::wstring& path) = 0;
    virtual std::unique_ptr<WritableFile> create(const std::wstring& path, CreateMode mode) = 0;
    virtual std::unique_ptr<ReadableFile> open(const std::wstring& path) = 0;
    // Fails if `to` exists and `replace` is false.
    virtual void rename(const std::wstring& from, const std::wstring& to, bool replace) = 0;
    // Best effort; returns false when the file could not be deleted.
    virtual bool remove(const std::wstring& path) noexcept = 0;
};

// The directory part of a full path ("C:\a\b.iso" -> "C:\a"); empty if there is none.
std::wstring parent_directory(const std::wstring& path);
// The file name part of a path ("C:\a\b.iso" -> "b.iso").
std::wstring file_name(const std::wstring& path);

class Win32FileSystem final : public FileSystem {
public:
    std::wstring full_path(const std::wstring& path) override;
    bool exists(const std::wstring& path) override;
    bool directory_exists(const std::wstring& path) override;
    std::optional<std::uint64_t> file_size(const std::wstring& path) override;
    VolumeInfo volume_info(const std::wstring& path) override;
    std::unique_ptr<WritableFile> create(const std::wstring& path, CreateMode mode) override;
    std::unique_ptr<ReadableFile> open(const std::wstring& path) override;
    void rename(const std::wstring& from, const std::wstring& to, bool replace) override;
    bool remove(const std::wstring& path) noexcept override;
};

}  // namespace isoforge::util
