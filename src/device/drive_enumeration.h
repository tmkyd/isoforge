// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace isoforge::device {

struct DriveEntry {
    std::uint32_t number = 0;         // N of \\.\CdRomN.
    std::optional<wchar_t> letter;    // Drive letter mapped to \Device\CdRomN, if any.

    bool operator==(const DriveEntry&) const = default;
};

// Builds the drive list from MS-DOS device names: every "CdRomN" name is a drive, and a drive
// letter whose target is "\Device\CdRomN" belongs to that drive. Sorted by number.
// `targets_of_letters[i]` is the QueryDosDevice target of drive letter 'A' + i (empty if none).
std::vector<DriveEntry> build_drive_list(const std::vector<std::wstring_view>& dos_device_names,
                                         const std::vector<std::wstring_view>& targets_of_letters);

// Enumerates optical drives with QueryDosDeviceW, including drives without a drive letter.
// Throws DeviceError when the device names cannot be read.
std::vector<DriveEntry> enumerate_optical_drives();

}  // namespace isoforge::device
