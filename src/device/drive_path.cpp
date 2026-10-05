// SPDX-License-Identifier: Apache-2.0

#include "device/drive_path.h"

#include <limits>

namespace isoforge::device {
namespace {

constexpr std::wstring_view kDevicePrefix = LR"(\\.\)";
constexpr std::wstring_view kCdRomPrefix = L"CdRom";

bool is_ascii_letter(wchar_t c) noexcept {
    return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
}

wchar_t to_upper_ascii(wchar_t c) noexcept {
    return (c >= L'a' && c <= L'z') ? static_cast<wchar_t>(c - L'a' + L'A') : c;
}

bool equals_ignoring_ascii_case(std::wstring_view a, std::wstring_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (to_upper_ascii(a[i]) != to_upper_ascii(b[i])) {
            return false;
        }
    }
    return true;
}

DrivePath drive_letter(wchar_t letter) {
    return DrivePath{DrivePath::Kind::DriveLetter, to_upper_ascii(letter), 0};
}

// "D", "D:" or "D:\".
std::optional<DrivePath> parse_letter(std::wstring_view text) {
    if (text.empty() || text.size() > 3 || !is_ascii_letter(text[0])) {
        return std::nullopt;
    }
    if (text.size() >= 2 && text[1] != L':') {
        return std::nullopt;
    }
    if (text.size() == 3 && text[2] != L'\\') {
        return std::nullopt;
    }
    return drive_letter(text[0]);
}

// "CdRomN" where N is a decimal number without leading zeros that fits in 32 bits.
std::optional<DrivePath> parse_cdrom(std::wstring_view text) {
    if (text.size() <= kCdRomPrefix.size() ||
        !equals_ignoring_ascii_case(text.substr(0, kCdRomPrefix.size()), kCdRomPrefix)) {
        return std::nullopt;
    }
    const std::wstring_view digits = text.substr(kCdRomPrefix.size());
    if (digits.size() > 1 && digits[0] == L'0') {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const wchar_t c : digits) {
        if (c < L'0' || c > L'9') {
            return std::nullopt;
        }
        value = value * 10 + static_cast<std::uint64_t>(c - L'0');
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            return std::nullopt;
        }
    }
    return DrivePath{DrivePath::Kind::CdRom, L'\0', static_cast<std::uint32_t>(value)};
}

}  // namespace

std::wstring DrivePath::device_path() const {
    std::wstring path(kDevicePrefix);
    if (kind == Kind::DriveLetter) {
        path += letter;
        path += L':';
    } else {
        path += kCdRomPrefix;
        path += std::to_wstring(number);
    }
    return path;
}

std::optional<DrivePath> parse_drive_path(std::wstring_view text) {
    if (text.starts_with(kDevicePrefix)) {
        const std::wstring_view rest = text.substr(kDevicePrefix.size());
        if (rest.size() == 2 && is_ascii_letter(rest[0]) && rest[1] == L':') {
            return drive_letter(rest[0]);
        }
        return parse_cdrom(rest);
    }
    if (std::optional<DrivePath> drive = parse_letter(text)) {
        return drive;
    }
    return parse_cdrom(text);
}

}  // namespace isoforge::device
