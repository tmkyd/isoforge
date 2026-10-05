// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include "device/drive_path.h"

namespace isoforge::cli {

// Help is the help command itself; it appears only as a help topic.
enum class Command { List, Info, Create, Verify, Help };

std::wstring_view command_name(Command command) noexcept;

enum class Verbosity { Quiet, Normal, Verbose };

inline constexpr std::uint32_t kDefaultRetries = 3;
inline constexpr std::uint32_t kDefaultWaitSeconds = 30;

// isoforge --help or isoforge help, or isoforge <command> --help or isoforge help <command> when
// topic is set.
struct HelpRequest {
    std::optional<Command> topic;
    // Shown because isoforge or a command that needs arguments was given none: on standard error
    // with exit code 1.
    bool instead_of_error = false;
};

struct VersionRequest {};

struct ListOptions {
    bool json = false;
};

struct InfoOptions {
    device::DrivePath drive;
    bool json = false;
    std::uint32_t wait_seconds = kDefaultWaitSeconds;  // --wait; 0 does not wait.
};

struct CreateOptions {
    device::DrivePath drive;
    std::wstring output;
    bool overwrite = false;
    bool keep_partial = false;
    // Defaults to <output>.sha256; std::nullopt when --no-hash-file is given.
    std::optional<std::wstring> hash_file;
    bool verify = false;
    std::uint32_t retries = kDefaultRetries;
    bool eject = false;
    std::optional<std::wstring> log_file;
    Verbosity verbosity = Verbosity::Normal;
    bool progress = true;
    std::uint32_t wait_seconds = kDefaultWaitSeconds;  // --wait; 0 does not wait.
    bool json = false;  // --json: the result as JSON on standard output.
};

struct VerifyOptions {
    std::wstring iso_file;
    // 64 hexadecimal digits or a hash file path. Defaults to <iso_file>.sha256.
    std::wstring hash;
    bool quiet = false;     // -q: only the Result line on standard output.
    bool progress = true;
};

using Invocation =
    std::variant<HelpRequest, VersionRequest, ListOptions, InfoOptions, CreateOptions, VerifyOptions>;

struct ParseResult {
    std::optional<Invocation> invocation;
    std::wstring error;              // Set when invocation is empty; marked for colors (cli/style.h).
    std::optional<Command> command;  // With an error: the command whose arguments were wrong.
    bool missing_required = false;   // The error lists missing required arguments.
};

// Parses the arguments that follow the program name.
ParseResult parse_command_line(std::span<const std::wstring_view> args);

}  // namespace isoforge::cli
