// SPDX-License-Identifier: Apache-2.0

#include "util/json.h"

#include <cwchar>

namespace isoforge::util {

std::wstring json_string(std::wstring_view text) {
    std::wstring result = L"\"";
    for (const wchar_t c : text) {
        switch (c) {
        case L'"':
            result += L"\\\"";
            break;
        case L'\\':
            result += L"\\\\";
            break;
        case L'\b':
            result += L"\\b";
            break;
        case L'\f':
            result += L"\\f";
            break;
        case L'\n':
            result += L"\\n";
            break;
        case L'\r':
            result += L"\\r";
            break;
        case L'\t':
            result += L"\\t";
            break;
        default:
            if (c < 0x20) {
                wchar_t escaped[8];
                std::swprintf(escaped, 8, L"\\u%04x", static_cast<unsigned>(c));
                result += escaped;
            } else {
                result += c;
            }
            break;
        }
    }
    result += L"\"";
    return result;
}

}  // namespace isoforge::util
