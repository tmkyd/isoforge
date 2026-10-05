// SPDX-License-Identifier: Apache-2.0

#include "cli/help.h"

#include <algorithm>
#include <span>
#include <string_view>
#include <vector>

#include "cli/style.h"

namespace isoforge::cli {
namespace {

// Help lines wrap at this width, as in uv's help.
constexpr std::size_t kWidth = 100;

struct Item {
    std::wstring_view name;  // "list", "<DRIVE>", "D, D:, D:\, \\.\D:"
    std::wstring_view description;
};

struct OptionItem {
    wchar_t short_name;           // L'\0' when there is no short form.
    std::wstring_view long_name;  // Without "--".
    std::wstring_view value;      // "<PATH>"; empty for flags.
    std::wstring_view description;
    std::wstring_view default_value;  // Shown as "[default: ...]"; empty for none.
};

struct CommandHelp {
    std::wstring_view about;
    std::wstring_view usage;           // After "Usage: ".
    std::wstring_view required_usage;  // Only the required arguments.
    std::span<const Item> arguments;
    std::span<const OptionItem> options;
};

constexpr OptionItem kHelpOption = {L'h', L"help", L"", L"Display the help for this command", L""};

constexpr Item kCommands[] = {
    {L"list", L"List optical drives"},
    {L"info", L"Show the disc layout and whether the disc is supported; does not create an ISO file"},
    {L"create",
     L"Save the logical sectors of a data disc to an ISO file as they are; it does not build an ISO file "
     L"from files"},
    {L"verify", L"Check the SHA-256 of an existing ISO file; does not use a disc"},
    {L"help", L"Display documentation for a command"},
};

constexpr OptionItem kTopOptions[] = {
    kHelpOption,
    {L'V', L"version", L"", L"Display the isoforge version", L""},
};

constexpr Item kExitCodes[] = {
    {L"0", L"Success"},
    {L"1", L"Usage error"},
    {L"2", L"Unsupported disc or layout, no disc, or not an optical drive"},
    {L"3", L"Read error"},
    {L"4", L"Output file error (including an existing file without --overwrite)"},
    {L"5", L"Hash mismatch"},
    {L"6", L"Insufficient permission"},
    {L"7", L"Other error"},
    {L"130", L"Interrupted"},
};

constexpr Item kDriveArgument[] = {
    {L"<DRIVE>", LR"(The optical drive: a drive letter (D, D:, D:\ or \\.\D:), or a device name (\\.\CdRom0 )"
                 LR"(or CdRom0) for a drive without a drive letter. Run `isoforge list` to see the drives)"}};

constexpr OptionItem kListOptions[] = {
    {L'\0', L"json", L"", L"Write machine-readable JSON", L""},
    kHelpOption,
};

constexpr OptionItem kInfoOptions[] = {
    {L'\0', L"json", L"", L"Write machine-readable JSON", L""},
    {L'\0', L"wait", L"<SECONDS>", L"Wait at most this long for the disc to become ready; 0 does not wait", L"30"},
    kHelpOption,
};

constexpr OptionItem kCreateOptions[] = {
    {L'o', L"output", L"<PATH>", L"Output ISO file", L""},
    {L'\0', L"overwrite", L"", L"Overwrite an existing ISO file or hash file", L""},
    {L'\0', L"keep-partial", L"", L"Keep the partial file after a failure or interruption", L""},
    {L'\0', L"hash-file", L"<PATH>", L"Write the SHA-256 hash file to this path", L"<output path>.sha256"},
    {L'\0', L"no-hash-file", L"", L"Do not write a hash file (the hash is still shown)", L""},
    {L'\0', L"verify", L"", L"Read the disc again after writing and compare the SHA-256", L""},
    {L'\0', L"retries", L"<N>", L"Retries for each failed read", L"3"},
    {L'\0', L"eject", L"", L"Eject the disc after success", L""},
    {L'\0', L"log", L"<PATH>", L"Write a detailed log file", L""},
    {L'\0', L"wait", L"<SECONDS>", L"Wait at most this long for the disc to become ready; 0 does not wait", L"30"},
    {L'\0', L"json", L"", L"Write the result as JSON to standard output (not with -v or -q)", L""},
    {L'v', L"verbose", L"", L"Show more output", L""},
    {L'q', L"quiet", L"", L"Show less output", L""},
    {L'\0', L"no-progress", L"", L"Do not show progress", L""},
    kHelpOption,
};

constexpr Item kVerifyArguments[] = {{L"<ISO_FILE>", L"The ISO file to check"}};

constexpr OptionItem kVerifyOptions[] = {
    {L'\0', L"hash", L"<HASH>", L"Expected SHA-256 (64 hexadecimal digits) or a hash file", L"<ISO_FILE>.sha256"},
    {L'q', L"quiet", L"", L"Show only the result line", L""},
    {L'\0', L"no-progress", L"", L"Do not show progress", L""},
    kHelpOption,
};

constexpr Item kHelpArguments[] = {{L"[COMMAND]", L"The command to show the help for"}};

constexpr OptionItem kHelpOnly[] = {kHelpOption};

CommandHelp help_of(Command command) noexcept {
    switch (command) {
    case Command::List:
        return {L"List optical drives with their drive letter, device name, vendor and model, whether a disc is "
                L"loaded, and the disc type",
                L"isoforge list [OPTIONS]", L"isoforge list", {}, kListOptions};
    case Command::Info:
        return {L"Show the disc type, session and track layout, capacity, file system range, and whether the disc "
                L"is supported; does not create an ISO file",
                L"isoforge info [OPTIONS] <DRIVE>", L"isoforge info <DRIVE>", kDriveArgument, kInfoOptions};
    case Command::Create:
        return {L"Save the logical sectors of a data disc to an ISO file as they are; it does not build an ISO "
                L"file from files (no re-authoring)",
                L"isoforge create [OPTIONS] --output <PATH> <DRIVE>", L"isoforge create --output <PATH> <DRIVE>",
                kDriveArgument, kCreateOptions};
    case Command::Verify:
        return {L"Compute the SHA-256 of an existing ISO file and compare it with the expected value; does not use "
                L"a disc. A hash file that names another file causes a warning",
                L"isoforge verify [OPTIONS] <ISO_FILE>", L"isoforge verify <ISO_FILE>", kVerifyArguments,
                kVerifyOptions};
    case Command::Help:
        break;
    }
    return {L"Display documentation for a command", L"isoforge help [COMMAND]", L"isoforge help", kHelpArguments,
            kHelpOnly};
}

// `text` broken into lines of at most `width` columns at spaces.
std::vector<std::wstring_view> wrap(std::wstring_view text, std::size_t width) {
    std::vector<std::wstring_view> lines;
    while (text.size() > width) {
        std::size_t cut = text.rfind(L' ', width);
        if (cut == std::wstring_view::npos || cut == 0) {
            cut = text.find(L' ', width);
            if (cut == std::wstring_view::npos) {
                break;
            }
        }
        lines.push_back(text.substr(0, cut));
        text.remove_prefix(cut + 1);
    }
    lines.push_back(text);
    return lines;
}

std::wstring heading(std::wstring_view text) {
    return mark(text, Style::Heading) + L"\n";
}

// Usage words: names and options are literals, <...> and [...] placeholders. Neighbouring
// literals form one span, as uv writes "isoforge create".
std::wstring usage_words(std::wstring_view usage) {
    std::wstring out;
    std::wstring literal;
    const auto append = [&](const std::wstring& part) { out += (out.empty() ? L"" : L" ") + part; };
    const auto flush = [&] {
        if (!literal.empty()) {
            append(mark(literal, Style::Literal));
            literal.clear();
        }
    };
    std::size_t begin = 0;
    while (begin < usage.size()) {
        std::size_t end = usage.find(L' ', begin);
        if (end == std::wstring_view::npos) {
            end = usage.size();
        }
        const std::wstring_view word = usage.substr(begin, end - begin);
        if (word.front() == L'<' || word.front() == L'[') {
            flush();
            append(mark(word, Style::Placeholder));
        } else {
            literal += (literal.empty() ? L"" : L" ") + std::wstring(word);
        }
        begin = end + 1;
    }
    flush();
    return out;
}

std::wstring marked_usage(std::wstring_view usage) {
    return mark(L"Usage:", Style::Heading) + L" " + usage_words(usage);
}

// A list entry: the marked name padded to `name_width` plain columns, then the wrapped description.
void add_entry(std::wstring& text, const std::wstring& marked_name, std::size_t plain_width, std::size_t name_width,
               std::wstring_view description) {
    const std::size_t column = 2 + name_width + 2;
    text += L"  " + marked_name;
    const std::vector<std::wstring_view> lines = wrap(description, kWidth - column);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        text += std::wstring(i == 0 ? name_width - plain_width + 2 : column, L' ');
        text += std::wstring(lines[i]) + L"\n";
    }
}

// Names separated by ", " are each a literal, as "-o, --output".
std::wstring literal_names(std::wstring_view names) {
    std::wstring out;
    std::size_t begin = 0;
    while (true) {
        const std::size_t comma = names.find(L", ", begin);
        out += mark(names.substr(begin, comma == std::wstring_view::npos ? std::wstring_view::npos : comma - begin),
                    Style::Literal);
        if (comma == std::wstring_view::npos) {
            return out;
        }
        out += L", ";
        begin = comma + 2;
    }
}

void add_items(std::wstring& text, std::wstring_view title, std::span<const Item> items, Style style) {
    text += L"\n" + heading(title);
    std::size_t width = 0;
    for (const Item& item : items) {
        width = std::max(width, item.name.size());
    }
    for (const Item& item : items) {
        const std::wstring name = style == Style::Literal ? literal_names(item.name) : mark(item.name, style);
        add_entry(text, name, item.name.size(), width, item.description);
    }
}

// "-o, --output <PATH>", or "    --overwrite" so that long names line up.
std::wstring option_name(const OptionItem& option, bool marked) {
    std::wstring name;
    if (option.short_name != L'\0') {
        const std::wstring short_form = std::wstring(L"-") + option.short_name;
        name += marked ? mark(short_form, Style::Literal) : short_form;
        name += L", ";
    } else {
        name += L"    ";
    }
    const std::wstring long_form = L"--" + std::wstring(option.long_name);
    name += marked ? mark(long_form, Style::Literal) : long_form;
    if (!option.value.empty()) {
        const std::wstring value = L" " + std::wstring(option.value);
        name += marked ? mark(value, Style::Placeholder) : value;
    }
    return name;
}

void add_options(std::wstring& text, std::span<const OptionItem> options) {
    text += L"\n" + heading(L"Options:");
    std::size_t width = 0;
    for (const OptionItem& option : options) {
        width = std::max(width, option_name(option, false).size());
    }
    for (const OptionItem& option : options) {
        std::wstring description(option.description);
        if (!option.default_value.empty()) {
            description += L" [default: " + std::wstring(option.default_value) + L"]";
        }
        add_entry(text, option_name(option, true), option_name(option, false).size(), width, description);
    }
}

std::wstring about(std::wstring_view text) {
    std::wstring out;
    for (const std::wstring_view line : wrap(text, kWidth)) {
        out += std::wstring(line) + L"\n";
    }
    return out;
}

std::wstring overview() {
    std::wstring text = about(L"Back up optical data discs to ISO files");
    text += L"\n" + marked_usage(L"isoforge [OPTIONS] <COMMAND>") + L"\n";
    add_items(text, L"Commands:", kCommands, Style::Literal);
    add_options(text, kTopOptions);
    add_items(text, L"Exit codes:", kExitCodes, Style::Literal);
    text += L"\nUse `isoforge help <command>` for more details.\n";
    return text;
}

std::wstring command_help(Command command) {
    const CommandHelp help = help_of(command);
    std::wstring text = about(help.about);
    text += L"\n" + marked_usage(help.usage) + L"\n";
    if (!help.arguments.empty()) {
        add_items(text, L"Arguments:", help.arguments, Style::Placeholder);
    }
    add_options(text, help.options);
    return text;
}

}  // namespace

std::wstring help_text(std::optional<Command> topic, bool color) {
    return render(topic ? command_help(*topic) : overview(), color);
}

std::wstring usage_line(std::optional<Command> command, bool color, bool required_only) {
    if (!command) {
        return render(marked_usage(L"isoforge [OPTIONS] <COMMAND>"), color);
    }
    const CommandHelp help = help_of(*command);
    return render(marked_usage(required_only ? help.required_usage : help.usage), color);
}

}  // namespace isoforge::cli
