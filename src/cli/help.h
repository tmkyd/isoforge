// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>
#include <string>

#include "cli/command_line.h"

namespace isoforge::cli {

// Help text in the layout of uv: isoforge --help (no topic) or isoforge <command> --help. With ANSI
// colors when `color` is set.
std::wstring help_text(std::optional<Command> topic, bool color);

// "Usage: isoforge info [OPTIONS] <DRIVE>"; for no command, "Usage: isoforge [OPTIONS] <COMMAND>".
// With `required_only`, only the required arguments, as clap shows them when some are missing:
// "Usage: isoforge info <DRIVE>". Without a newline; with ANSI colors when `color` is set.
std::wstring usage_line(std::optional<Command> command, bool color, bool required_only = false);

}  // namespace isoforge::cli
