// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <string_view>

namespace isoforge::util {

// Converts UTF-16 to UTF-8. Unpaired surrogates become U+FFFD.
std::string to_utf8(std::wstring_view text);

// Converts UTF-8 to UTF-16. Invalid sequences become U+FFFD.
std::wstring from_utf8(std::string_view text);

}  // namespace isoforge::util
