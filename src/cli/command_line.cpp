// SPDX-License-Identifier: Apache-2.0

#include "cli/command_line.h"

#include <limits>
#include <map>
#include <vector>

#include "cli/style.h"

namespace isoforge::cli {
namespace {

struct UsageError {
    std::wstring message;
    std::optional<Command> command;  // Set once the command is known.
    bool missing_required = false;
};

struct OptionSpec {
    std::wstring_view name;   // Long name without "--".
    wchar_t short_name;       // L'\0' when there is no short form.
    std::wstring_view value;  // The placeholder of the value, such as "<PATH>"; empty for flags.

    bool takes_value() const noexcept { return !value.empty(); }
};

// Options that isoforge deliberately does not provide (--force, --skip-errors, --resume, ...) are
// rejected as unknown options.
constexpr OptionSpec kListOptions[] = {
    {L"json", L'\0', L""},
};

constexpr OptionSpec kInfoOptions[] = {
    {L"json", L'\0', L""},
    {L"wait", L'\0', L"<SECONDS>"},
};

constexpr OptionSpec kCreateOptions[] = {
    {L"output", L'o', L"<PATH>"},
    {L"overwrite", L'\0', L""},
    {L"keep-partial", L'\0', L""},
    {L"hash-file", L'\0', L"<PATH>"},
    {L"no-hash-file", L'\0', L""},
    {L"verify", L'\0', L""},
    {L"retries", L'\0', L"<N>"},
    {L"eject", L'\0', L""},
    {L"log", L'\0', L"<PATH>"},
    {L"verbose", L'v', L""},
    {L"quiet", L'q', L""},
    {L"no-progress", L'\0', L""},
    {L"wait", L'\0', L"<SECONDS>"},
    {L"json", L'\0', L""},
};

constexpr OptionSpec kVerifyOptions[] = {
    {L"hash", L'\0', L"<HASH>"},
    {L"quiet", L'q', L""},
    {L"no-progress", L'\0', L""},
};

constexpr std::wstring_view kHashFileSuffix = L".sha256";

struct RawArguments {
    std::vector<std::wstring_view> positionals;
    std::map<std::wstring_view, std::wstring_view> options;  // Long name -> value ("" for flags).

    bool has(std::wstring_view name) const {
        return options.contains(name);
    }

    std::optional<std::wstring_view> value(std::wstring_view name) const {
        const auto it = options.find(name);
        if (it == options.end()) {
            return std::nullopt;
        }
        return it->second;
    }
};

// Parts of the error messages, quoted and marked as clap colors them: a value the user gave is
// invalid (yellow), a name of the command is a literal (bold cyan).
std::wstring invalid(std::wstring_view text) {
    return L"'" + mark(text, Style::Invalid) + L"'";
}

std::wstring literal(std::wstring_view text) {
    return L"'" + mark(text, Style::Literal) + L"'";
}

// "--output <PATH>" or "--eject".
std::wstring display(const OptionSpec& spec) {
    std::wstring text = L"--" + std::wstring(spec.name);
    if (spec.takes_value()) {
        text += L" " + std::wstring(spec.value);
    }
    return text;
}

std::wstring unexpected_argument(std::wstring_view argument) {
    return L"unexpected argument " + invalid(argument) + L" found";
}

std::wstring conflict(std::wstring_view first, std::wstring_view second) {
    return L"the argument " + invalid(first) + L" cannot be used with " + invalid(second);
}

// A clap-style tip on how to name a drive, within 100 columns.
std::wstring drive_tip() {
    return L"\n\n  " + mark(L"tip:", Style::Valid) +
           L" <DRIVE> is a drive letter such as D: or a device name such as CdRom0; run '" +
           mark(L"isoforge list", Style::Valid) + L"'\n       to see the drives";
}

// clap lists every missing argument on its own line.
UsageError missing_arguments(const std::vector<std::wstring>& names) {
    std::wstring text = L"the following required arguments were not provided:";
    bool drive = false;
    for (const std::wstring& name : names) {
        text += L"\n  " + mark(name, Style::Valid);
        drive = drive || name == L"<DRIVE>";
    }
    if (drive) {
        text += drive_tip();
    }
    return UsageError{text, std::nullopt, true};
}

bool is_help(std::wstring_view arg) noexcept {
    return arg == L"--help" || arg == L"-h";
}

const OptionSpec* find_option(std::span<const OptionSpec> specs, std::wstring_view long_name) {
    for (const OptionSpec& spec : specs) {
        if (spec.name == long_name) {
            return &spec;
        }
    }
    return nullptr;
}

const OptionSpec* find_option(std::span<const OptionSpec> specs, wchar_t short_name) {
    for (const OptionSpec& spec : specs) {
        if (spec.short_name != L'\0' && spec.short_name == short_name) {
            return &spec;
        }
    }
    return nullptr;
}

std::span<const OptionSpec> options_of(Command command) noexcept {
    switch (command) {
    case Command::List:
        return kListOptions;
    case Command::Info:
        return kInfoOptions;
    case Command::Create:
        return kCreateOptions;
    case Command::Verify:
        return kVerifyOptions;
    case Command::Help:
        break;
    }
    return {};
}

// True when -h or --help appears as an option: not after "--", and not as the value of an option
// such as "-o -h".
bool contains_help(std::span<const std::wstring_view> args, std::span<const OptionSpec> specs) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::wstring_view arg = args[i];
        if (arg == L"--") {
            return false;
        }
        if (is_help(arg)) {
            return true;
        }
        const OptionSpec* spec = nullptr;
        if (arg.starts_with(L"--") && arg.find(L'=') == std::wstring_view::npos) {
            spec = find_option(specs, arg.substr(2));
        } else if (arg.size() == 2 && arg[0] == L'-') {
            spec = find_option(specs, arg[1]);
        }
        if (spec != nullptr && spec->takes_value()) {
            ++i;  // Skip the option's value.
        }
    }
    return false;
}

// Splits arguments into positionals and options. Options may appear before or after positionals,
// take values as "--name value", "--name=value" or "-o value", and "--" ends option parsing.
RawArguments collect(std::span<const std::wstring_view> args, std::span<const OptionSpec> specs) {
    RawArguments raw;
    bool only_positionals = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::wstring_view arg = args[i];
        if (only_positionals || arg.size() < 2 || arg[0] != L'-') {
            raw.positionals.push_back(arg);
            continue;
        }
        if (arg == L"--") {
            only_positionals = true;
            continue;
        }

        const OptionSpec* spec = nullptr;
        std::wstring_view token = arg;
        std::optional<std::wstring_view> inline_value;
        if (arg.starts_with(L"--")) {
            std::wstring_view name = arg.substr(2);
            if (const std::size_t equals = name.find(L'='); equals != std::wstring_view::npos) {
                inline_value = name.substr(equals + 1);
                name = name.substr(0, equals);
                token = arg.substr(0, equals + 2);
            }
            spec = find_option(specs, name);
        } else if (arg.size() == 2) {
            spec = find_option(specs, arg[1]);
        }
        if (spec == nullptr) {
            throw UsageError{unexpected_argument(token)};
        }
        if (raw.has(spec->name)) {
            throw UsageError{L"the argument " + invalid(display(*spec)) + L" cannot be used multiple times"};
        }

        std::wstring_view value;
        if (spec->takes_value()) {
            if (inline_value) {
                value = *inline_value;
            } else if (i + 1 < args.size()) {
                value = args[++i];
            }
            if (value.empty()) {
                throw UsageError{L"a value is required for " + invalid(display(*spec)) + L" but none was supplied"};
            }
        } else if (inline_value) {
            throw UsageError{L"unexpected value " + invalid(*inline_value) + L" for " + literal(display(*spec)) +
                             L" found; no more were expected"};
        }
        raw.options.emplace(spec->name, value);
    }
    return raw;
}

void expect_no_positionals(const RawArguments& raw) {
    if (!raw.positionals.empty()) {
        throw UsageError{unexpected_argument(raw.positionals.front())};
    }
}

// The one positional argument named `what`, such as "<DRIVE>". `also_missing` lists required
// options that are missing too, so that all of them are reported at once.
std::wstring_view single_positional(const RawArguments& raw, std::wstring_view what,
                                    std::vector<std::wstring> also_missing = {}) {
    // As clap: an extra argument is found while parsing, before the required ones are checked.
    if (raw.positionals.size() > 1) {
        throw UsageError{unexpected_argument(raw.positionals[1])};
    }
    if (raw.positionals.empty()) {
        also_missing.insert(also_missing.begin(), std::wstring(what));
    }
    if (!also_missing.empty()) {
        throw missing_arguments(also_missing);
    }
    return raw.positionals.front();
}

device::DrivePath single_drive(const RawArguments& raw, std::vector<std::wstring> also_missing = {}) {
    const std::wstring_view text = single_positional(raw, L"<DRIVE>", std::move(also_missing));
    const std::optional<device::DrivePath> drive = device::parse_drive_path(text);
    if (!drive) {
        throw UsageError{L"invalid value " + invalid(text) + L" for " + literal(L"<DRIVE>") + drive_tip()};
    }
    return *drive;
}

// A non-negative decimal integer in 32 bits, for --retries and --wait.
std::uint32_t parse_count(std::wstring_view text, const OptionSpec& option) {
    std::uint64_t value = 0;
    bool valid = !text.empty();
    for (const wchar_t c : text) {
        if (c < L'0' || c > L'9') {
            valid = false;
            break;
        }
        value = value * 10 + static_cast<std::uint64_t>(c - L'0');
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            valid = false;
            break;
        }
    }
    if (!valid) {
        throw UsageError{L"invalid value " + invalid(text) + L" for " + literal(display(option)) +
                         L": expected a non-negative integer"};
    }
    return static_cast<std::uint32_t>(value);
}

ListOptions parse_list(std::span<const std::wstring_view> args) {
    const RawArguments raw = collect(args, kListOptions);
    expect_no_positionals(raw);
    return ListOptions{raw.has(L"json")};
}

std::uint32_t wait_seconds(const RawArguments& raw) {
    const std::optional<std::wstring_view> wait = raw.value(L"wait");
    return wait ? parse_count(*wait, *find_option(kInfoOptions, L"wait")) : kDefaultWaitSeconds;
}

InfoOptions parse_info(std::span<const std::wstring_view> args) {
    const RawArguments raw = collect(args, kInfoOptions);
    return InfoOptions{single_drive(raw), raw.has(L"json"), wait_seconds(raw)};
}

CreateOptions parse_create(std::span<const std::wstring_view> args) {
    const RawArguments raw = collect(args, kCreateOptions);

    CreateOptions options;
    const std::optional<std::wstring_view> output = raw.value(L"output");
    std::vector<std::wstring> missing;
    if (!output) {
        missing.push_back(display(*find_option(kCreateOptions, L"output")));
    }
    options.drive = single_drive(raw, std::move(missing));
    options.output = *output;

    options.overwrite = raw.has(L"overwrite");
    options.keep_partial = raw.has(L"keep-partial");

    const std::optional<std::wstring_view> hash_file = raw.value(L"hash-file");
    if (raw.has(L"no-hash-file")) {
        if (hash_file) {
            throw UsageError{conflict(L"--hash-file <PATH>", L"--no-hash-file")};
        }
        options.hash_file = std::nullopt;
    } else if (hash_file) {
        options.hash_file = std::wstring(*hash_file);
    } else {
        options.hash_file = options.output + std::wstring(kHashFileSuffix);
    }

    options.verify = raw.has(L"verify");
    if (const std::optional<std::wstring_view> retries = raw.value(L"retries")) {
        options.retries = parse_count(*retries, *find_option(kCreateOptions, L"retries"));
    }
    options.wait_seconds = wait_seconds(raw);
    options.eject = raw.has(L"eject");
    if (const std::optional<std::wstring_view> log = raw.value(L"log")) {
        options.log_file = std::wstring(*log);
    }

    if (raw.has(L"verbose") && raw.has(L"quiet")) {
        throw UsageError{conflict(L"--verbose", L"--quiet")};
    }
    if (raw.has(L"verbose")) {
        options.verbosity = Verbosity::Verbose;
    } else if (raw.has(L"quiet")) {
        options.verbosity = Verbosity::Quiet;
    }
    // --json reserves standard output for the JSON result.
    options.json = raw.has(L"json");
    if (options.json && (raw.has(L"verbose") || raw.has(L"quiet"))) {
        throw UsageError{conflict(L"--json", raw.has(L"verbose") ? L"--verbose" : L"--quiet")};
    }
    options.progress = !raw.has(L"no-progress");
    return options;
}

VerifyOptions parse_verify(std::span<const std::wstring_view> args) {
    const RawArguments raw = collect(args, kVerifyOptions);
    VerifyOptions options;
    options.iso_file = single_positional(raw, L"<ISO_FILE>");
    if (const std::optional<std::wstring_view> hash = raw.value(L"hash")) {
        options.hash = *hash;
    } else {
        options.hash = options.iso_file + std::wstring(kHashFileSuffix);
    }
    options.quiet = raw.has(L"quiet");
    options.progress = !raw.has(L"no-progress");
    return options;
}

std::optional<Command> find_command(std::wstring_view name) {
    for (const Command command : {Command::List, Command::Info, Command::Create, Command::Verify, Command::Help}) {
        if (command_name(command) == name) {
            return command;
        }
    }
    return std::nullopt;
}

// isoforge help [<command>]: the overview, or the help of a command.
HelpRequest parse_help(std::span<const std::wstring_view> args) {
    if (contains_help(args, {})) {
        return HelpRequest{Command::Help};
    }
    if (args.empty()) {
        return HelpRequest{};
    }
    if (args.front().starts_with(L'-')) {
        throw UsageError{unexpected_argument(args.front())};
    }
    if (args.size() > 1) {
        throw UsageError{unexpected_argument(args[1])};
    }
    const std::optional<Command> topic = find_command(args.front());
    if (!topic) {
        throw UsageError{L"unrecognized subcommand " + invalid(args.front())};
    }
    return HelpRequest{*topic};
}

// Commands that cannot run without arguments; given none, they show their help.
bool requires_arguments(Command command) noexcept {
    return command == Command::Info || command == Command::Create || command == Command::Verify;
}

Invocation parse(std::span<const std::wstring_view> args) {
    if (args.empty()) {
        return HelpRequest{std::nullopt, true};
    }

    const std::wstring_view first = args.front();
    if (is_help(first)) {
        return HelpRequest{};
    }
    if (first == L"--version" || first == L"-V") {
        if (args.size() > 1) {
            throw UsageError{unexpected_argument(args[1])};
        }
        return VersionRequest{};
    }
    if (first.starts_with(L'-')) {
        throw UsageError{unexpected_argument(first)};
    }

    const std::optional<Command> command = find_command(first);
    if (!command) {
        throw UsageError{L"unrecognized subcommand " + invalid(first)};
    }

    const std::span<const std::wstring_view> rest = args.subspan(1);
    try {
        if (*command == Command::Help) {
            return parse_help(rest);
        }
        if (rest.empty() && requires_arguments(*command)) {
            return HelpRequest{*command, true};
        }
        if (contains_help(rest, options_of(*command))) {
            return HelpRequest{*command};
        }
        switch (*command) {
        case Command::List:
            return parse_list(rest);
        case Command::Info:
            return parse_info(rest);
        case Command::Create:
            return parse_create(rest);
        case Command::Verify:
        case Command::Help:
            break;
        }
        return parse_verify(rest);
    } catch (UsageError& error) {
        error.command = *command;
        throw;
    }
}

}  // namespace

std::wstring_view command_name(Command command) noexcept {
    switch (command) {
    case Command::List:
        return L"list";
    case Command::Info:
        return L"info";
    case Command::Create:
        return L"create";
    case Command::Verify:
        return L"verify";
    case Command::Help:
        break;
    }
    return L"help";
}

ParseResult parse_command_line(std::span<const std::wstring_view> args) {
    try {
        return ParseResult{parse(args), {}, std::nullopt, false};
    } catch (const UsageError& error) {
        return ParseResult{std::nullopt, error.message, error.command, error.missing_required};
    }
}

}  // namespace isoforge::cli
