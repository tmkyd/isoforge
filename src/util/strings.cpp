// SPDX-License-Identifier: Apache-2.0

#include "util/strings.h"

#include <cwctype>
#include <format>

namespace isoforge::util {
namespace {

bool is_printable_ascii(std::uint32_t c) noexcept {
    return c >= 0x20 && c < 0x7F;
}

}  // namespace

std::wstring widen_ascii(std::string_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        result += byte < 0x80 ? static_cast<wchar_t>(byte) : L'?';
    }
    return result;
}

std::string narrow_ascii(std::wstring_view text) {
    std::string result;
    result.reserve(text.size());
    for (const wchar_t c : text) {
        result += is_printable_ascii(c) ? static_cast<char>(c) : '?';
    }
    return result;
}

std::wstring printable_ascii(std::span<const std::uint8_t> bytes) {
    std::wstring result;
    result.reserve(bytes.size());
    for (const std::uint8_t b : bytes) {
        result += is_printable_ascii(b) ? static_cast<wchar_t>(b) : L'?';
    }
    return result;
}

std::string hex_with_suffix(std::uint32_t value, int digits) {
    return std::format("{:0{}X}h", value, digits);
}

bool equals_ignore_case(std::wstring_view a, std::wstring_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::towupper(a[i]) != std::towupper(b[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace isoforge::util
