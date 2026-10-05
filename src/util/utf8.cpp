// SPDX-License-Identifier: Apache-2.0

#include "util/utf8.h"

#include <windows.h>

#include <limits>
#include <stdexcept>
#include <system_error>

namespace isoforge::util {

std::string to_utf8(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("to_utf8: input is too long");
    }
    const int length = static_cast<int>(text.size());
    const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "WideCharToMultiByte");
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    if (::WideCharToMultiByte(CP_UTF8, 0, text.data(), length, result.data(), size, nullptr, nullptr) !=
        size) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(),
                                "WideCharToMultiByte");
    }
    return result;
}

std::wstring from_utf8(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::length_error("from_utf8: input is too long");
    }
    const int length = static_cast<int>(text.size());
    const int size = ::MultiByteToWideChar(CP_UTF8, 0, text.data(), length, nullptr, 0);
    if (size <= 0) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "MultiByteToWideChar");
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (::MultiByteToWideChar(CP_UTF8, 0, text.data(), length, result.data(), size) != size) {
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "MultiByteToWideChar");
    }
    return result;
}

}  // namespace isoforge::util
