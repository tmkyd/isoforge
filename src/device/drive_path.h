// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace isoforge::device {

// An optical drive given on the command line.
struct DrivePath {
    enum class Kind { DriveLetter, CdRom };

    Kind kind = Kind::DriveLetter;
    wchar_t letter = L'\0';     // Upper-case 'A'-'Z' when kind is DriveLetter.
    std::uint32_t number = 0;   // N of CdRomN when kind is CdRom.

    // Win32 device name for CreateFileW, e.g. L"\\\\.\\D:" or L"\\\\.\\CdRom0".
    std::wstring device_path() const;

    bool operator==(const DrivePath&) const = default;
};

// Accepts, ignoring case: D, D:, D:\, \\.\D:, \\.\CdRomN and CdRomN.
// Returns std::nullopt for anything else (including \\?\ paths and \\.\PhysicalDriveN).
std::optional<DrivePath> parse_drive_path(std::wstring_view text);

}  // namespace isoforge::device
