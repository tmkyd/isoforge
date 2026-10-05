// SPDX-License-Identifier: Apache-2.0

#include "device/drive_enumeration.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "device/device_error.h"
#include "device/drive_path.h"

namespace isoforge::device {
namespace {

constexpr std::wstring_view kDevicePrefix = LR"(\Device\)";

// The CdRom number of a name such as "CdRom0", or nothing.
std::optional<std::uint32_t> cdrom_number(std::wstring_view name) {
    const std::optional<DrivePath> drive = parse_drive_path(name);
    if (!drive || drive->kind != DrivePath::Kind::CdRom) {
        return std::nullopt;
    }
    return drive->number;
}

// Calls QueryDosDeviceW, growing the buffer as needed. Returns the NUL-separated list.
std::wstring query_dos_device(const wchar_t* name) {
    std::wstring buffer(4096, L'\0');
    for (;;) {
        const DWORD length = ::QueryDosDeviceW(name, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length != 0) {
            buffer.resize(length);
            return buffer;
        }
        const DWORD error = ::GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER || buffer.size() >= (1u << 24)) {
            throw DeviceError::from_win32(error, "QueryDosDeviceW", name == nullptr ? "(all)" : "(drive)");
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::vector<std::wstring_view> split_nul(const std::wstring& list) {
    std::vector<std::wstring_view> items;
    std::size_t begin = 0;
    while (begin < list.size()) {
        const std::size_t end = list.find(L'\0', begin);
        const std::size_t stop = end == std::wstring::npos ? list.size() : end;
        if (stop > begin) {
            items.emplace_back(list.data() + begin, stop - begin);
        }
        begin = stop + 1;
    }
    return items;
}

}  // namespace

std::vector<DriveEntry> build_drive_list(const std::vector<std::wstring_view>& dos_device_names,
                                         const std::vector<std::wstring_view>& targets_of_letters) {
    std::vector<DriveEntry> drives;
    for (const std::wstring_view name : dos_device_names) {
        const std::optional<std::uint32_t> number = cdrom_number(name);
        if (number && std::none_of(drives.begin(), drives.end(),
                                   [&](const DriveEntry& d) { return d.number == *number; })) {
            drives.push_back(DriveEntry{*number, std::nullopt});
        }
    }
    for (std::size_t i = 0; i < targets_of_letters.size() && i < 26; ++i) {
        const std::wstring_view target = targets_of_letters[i];
        if (!target.starts_with(kDevicePrefix)) {
            continue;
        }
        const std::optional<std::uint32_t> number = cdrom_number(target.substr(kDevicePrefix.size()));
        if (!number) {
            continue;
        }
        for (DriveEntry& drive : drives) {
            if (drive.number == *number && !drive.letter) {
                drive.letter = static_cast<wchar_t>(L'A' + i);
            }
        }
    }
    std::sort(drives.begin(), drives.end(), [](const DriveEntry& a, const DriveEntry& b) { return a.number < b.number; });
    return drives;
}

std::vector<DriveEntry> enumerate_optical_drives() {
    const std::wstring all = query_dos_device(nullptr);
    const std::vector<std::wstring_view> names = split_nul(all);

    std::vector<std::wstring> targets(26);
    const DWORD logical = ::GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if ((logical & (1u << i)) == 0) {
            continue;
        }
        const wchar_t letter[] = {static_cast<wchar_t>(L'A' + i), L':', L'\0'};
        wchar_t target[MAX_PATH];
        const DWORD length = ::QueryDosDeviceW(letter, target, MAX_PATH);
        if (length != 0) {
            targets[i] = target;  // The first NUL-terminated string is the current target.
        }
    }
    const std::vector<std::wstring_view> target_views(targets.begin(), targets.end());
    return build_drive_list(names, target_views);
}

}  // namespace isoforge::device
