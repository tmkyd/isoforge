// SPDX-License-Identifier: Apache-2.0

#include "cli/style.h"

#include "cli/help.h"

namespace isoforge::cli {
namespace {

// A mark starts with kBegin + the style number and ends with kEnd.
constexpr wchar_t kBegin = 0xE000;
constexpr wchar_t kEnd = 0xE0FF;
constexpr int kStyleCount = static_cast<int>(Style::Bold) + 1;

// The sequences uv writes.
std::wstring_view sequence(Style style) noexcept {
    switch (style) {
    case Style::Heading:
        return L"\x1b[1m\x1b[32m";
    case Style::Literal:
        return L"\x1b[1m\x1b[36m";
    case Style::Placeholder:
        return L"\x1b[36m";
    case Style::Invalid:
        return L"\x1b[33m";
    case Style::Valid:
        return L"\x1b[32m";
    case Style::Error:
        return L"\x1b[1m\x1b[31m";
    case Style::Warning:
        return L"\x1b[1m\x1b[33m";
    case Style::Bold:
        return L"\x1b[1m";
    }
    return L"";
}

}  // namespace

std::wstring mark(std::wstring_view text, Style style) {
    std::wstring marked;
    marked += static_cast<wchar_t>(kBegin + static_cast<int>(style));
    marked += text;
    marked += kEnd;
    return marked;
}

std::wstring render(std::wstring_view marked, bool color) {
    std::wstring out;
    out.reserve(marked.size());
    for (const wchar_t c : marked) {
        if (c >= kBegin && c < kBegin + kStyleCount) {
            if (color) {
                out += sequence(static_cast<Style>(c - kBegin));
            }
        } else if (c == kEnd) {
            if (color) {
                out += L"\x1b[0m";
            }
        } else {
            out += c;
        }
    }
    return out;
}

void write_error(Console& console, std::wstring_view message) {
    console.write_err(render(mark(L"error", Style::Error) + mark(L":", Style::Bold) + L" " + std::wstring(message) +
                                 L"\n",
                             console.err_has_color()));
}

void write_warning(Console& console, std::wstring_view message) {
    console.write_err(render(mark(L"warning", Style::Warning) + mark(L":", Style::Bold) + L" " +
                                 mark(message, Style::Bold) + L"\n",
                             console.err_has_color()));
}

void write_usage_error(Console& console, std::wstring_view message, std::optional<Command> command,
                       bool missing_required) {
    const bool color = console.err_has_color();
    console.write_err(render(mark(L"error:", Style::Error) + L" " + std::wstring(message) + L"\n\n", color) +
                      usage_line(command, color, missing_required) + L"\n\n" +
                      render(L"For more information, try '" + mark(L"--help", Style::Literal) + L"'.\n", color));
}

}  // namespace isoforge::cli
