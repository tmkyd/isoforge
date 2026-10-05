// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <string_view>

namespace isoforge::util {

// A JSON string literal (with quotes) for `text`, escaping quotes, backslashes and control
// characters. Other characters are kept; the output is UTF-8 encoded when written.
std::wstring json_string(std::wstring_view text);

}  // namespace isoforge::util
