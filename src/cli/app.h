// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <span>
#include <string_view>

#include "cli/console.h"

namespace isoforge::cli {

struct Services;

// Runs isoforge with the arguments that follow the program name and returns the exit code.
// Uses the real drives and files, and handles Ctrl+C.
int run(std::span<const std::wstring_view> args, Console& console);

// Same with every outside dependency supplied, without Ctrl+C handling (tests pass fakes).
int run(std::span<const std::wstring_view> args, Console& console, const Services& services);

}  // namespace isoforge::cli
