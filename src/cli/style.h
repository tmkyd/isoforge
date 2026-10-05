// SPDX-License-Identifier: Apache-2.0
//
// Colors and prefixes of the console output, as uv (clap) writes them.
//
// Text is built with marks around its styled parts (mark()) and turned into ANSI sequences, or
// into plain text, only when it is written (render()). The marks are private-use characters.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "cli/command_line.h"
#include "cli/console.h"

namespace isoforge::cli {

enum class Style {
    Heading,      // Bold green: "Usage:", "Options:".
    Literal,      // Bold cyan: command and option names.
    Placeholder,  // Cyan: <DRIVE>, [OPTIONS].
    Invalid,      // Yellow: a value the user gave that is wrong.
    Valid,        // Green: what is expected, such as a missing argument.
    Error,        // Bold red: "error".
    Warning,      // Bold yellow: "warning".
    Bold,
};

// `text` with marks for `style`.
std::wstring mark(std::wstring_view text, Style style);

// Marked text with the marks turned into ANSI sequences, or removed when `color` is false.
std::wstring render(std::wstring_view marked, bool color);

// Writes "error: <message>" or "warning: <message>" to standard error. `message` may be marked.
void write_error(Console& console, std::wstring_view message);
void write_warning(Console& console, std::wstring_view message);

// Writes an argument error: the marked message, the usage line of `command` (the overview without
// one; only the required arguments with `missing_required`) and how to get more help.
void write_usage_error(Console& console, std::wstring_view message, std::optional<Command> command,
                       bool missing_required = false);

}  // namespace isoforge::cli
