// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace isoforge::util {

// ASCII text such as exception messages as UTF-16; bytes outside ASCII become '?'.
std::wstring widen_ascii(std::string_view text);

// UTF-16 text as ASCII, e.g. device names for exception messages; characters outside printable
// ASCII become '?'.
std::string narrow_ascii(std::wstring_view text);

// Bytes reported by a device (identification strings) as text; bytes outside printable ASCII
// become '?'.
std::wstring printable_ascii(std::span<const std::uint8_t> bytes);

// Upper-case hexadecimal with at least `digits` digits and an "h" suffix, as MMC writes codes:
// hex_with_suffix(0x11, 4) is "0011h".
std::string hex_with_suffix(std::uint32_t value, int digits);

// Case-insensitive comparison for paths and file names (towupper per character).
bool equals_ignore_case(std::wstring_view a, std::wstring_view b) noexcept;

}  // namespace isoforge::util
