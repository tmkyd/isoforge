// SPDX-License-Identifier: Apache-2.0

#include "cli/console.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "util/utf8.h"

namespace isoforge::cli {
namespace {

constexpr std::size_t kChunkSize = 8192;

bool is_high_surrogate(wchar_t c) noexcept {
    return c >= 0xD800 && c <= 0xDBFF;
}

// Output errors cannot be reported anywhere else, so they stop the write silently.
void write_to(DWORD standard_handle, std::wstring_view text) {
    const HANDLE handle = ::GetStdHandle(standard_handle);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE || text.empty()) {
        return;
    }

    DWORD mode = 0;
    if (::GetConsoleMode(handle, &mode)) {
        while (!text.empty()) {
            std::size_t chunk = std::min(text.size(), kChunkSize);
            if (chunk < text.size() && is_high_surrogate(text[chunk - 1])) {
                --chunk;  // Keep surrogate pairs together.
            }
            DWORD written = 0;
            if (!::WriteConsoleW(handle, text.data(), static_cast<DWORD>(chunk), &written, nullptr) ||
                written == 0) {
                return;
            }
            text.remove_prefix(written);
        }
        return;
    }

    const std::string bytes = util::to_utf8(text);
    std::string_view rest = bytes;
    while (!rest.empty()) {
        const std::size_t chunk = std::min(rest.size(), kChunkSize);
        DWORD written = 0;
        if (!::WriteFile(handle, rest.data(), static_cast<DWORD>(chunk), &written, nullptr) ||
            written == 0) {
            return;
        }
        rest.remove_prefix(written);
    }
}

// Enables ANSI sequences on a console handle. Returns whether colors can be written; `restore`
// receives the previous mode when it was changed.
bool enable_color(DWORD standard_handle, std::optional<std::uint32_t>& restore) {
    const HANDLE handle = ::GetStdHandle(standard_handle);
    DWORD mode = 0;
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE || !::GetConsoleMode(handle, &mode)) {
        return false;  // Redirected: no colors.
    }
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0) {
        return true;
    }
    if (!::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        return false;
    }
    restore = mode;
    return true;
}

void restore_mode(DWORD standard_handle, const std::optional<std::uint32_t>& mode) {
    if (mode) {
        ::SetConsoleMode(::GetStdHandle(standard_handle), *mode);
    }
}

// NO_COLOR (https://no-color.org): set to a non-empty value, it turns colors off.
bool no_color_requested() {
    wchar_t value[2];
    return ::GetEnvironmentVariableW(L"NO_COLOR", value, 2) > 0;
}

}  // namespace

SystemConsole::SystemConsole() {
    if (!no_color_requested()) {
        out_color_ = enable_color(STD_OUTPUT_HANDLE, out_mode_);
        err_color_ = enable_color(STD_ERROR_HANDLE, err_mode_);
    }
}

SystemConsole::~SystemConsole() {
    // In the reverse order: when both handles are the same console, the first change is undone last.
    restore_mode(STD_ERROR_HANDLE, err_mode_);
    restore_mode(STD_OUTPUT_HANDLE, out_mode_);
}

void SystemConsole::write_out(std::wstring_view text) {
    write_to(STD_OUTPUT_HANDLE, text);
}

void SystemConsole::write_err(std::wstring_view text) {
    write_to(STD_ERROR_HANDLE, text);
}

bool SystemConsole::err_is_terminal() const {
    DWORD mode = 0;
    const HANDLE handle = ::GetStdHandle(STD_ERROR_HANDLE);
    return handle != nullptr && handle != INVALID_HANDLE_VALUE && ::GetConsoleMode(handle, &mode);
}

}  // namespace isoforge::cli
