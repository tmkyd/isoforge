// SPDX-License-Identifier: Apache-2.0

#include "cli/app.h"

#include <exception>
#include <string>
#include <variant>

#include "cli/command_line.h"
#include "cli/create_command.h"
#include "cli/device_errors.h"
#include "cli/drive_provider.h"
#include "cli/exit_code.h"
#include "cli/help.h"
#include "cli/info_command.h"
#include "cli/list_command.h"
#include "cli/services.h"
#include "cli/style.h"
#include "cli/verify_command.h"
#include "device/device_error.h"
#include "util/file_system.h"
#include "util/strings.h"
#include "util/version.h"

namespace isoforge::cli {
namespace {

// The arguments as one line for the log, quoting those with spaces.
std::wstring command_line(std::span<const std::wstring_view> args) {
    std::wstring line = L"isoforge";
    for (const std::wstring_view arg : args) {
        line += L' ';
        if (arg.empty() || arg.find_first_of(L" \t") != std::wstring_view::npos) {
            line += L'"' + std::wstring(arg) + L'"';
        } else {
            line += arg;
        }
    }
    return line;
}

int dispatch(const Invocation& invocation, Console& console, const Services& services,
             std::span<const std::wstring_view> args) {
    if (const auto* help = std::get_if<HelpRequest>(&invocation)) {
        if (help->instead_of_error) {
            // No arguments where some are needed: the help instead of an error.
            console.write_err(help_text(help->topic, console.err_has_color()));
            return to_int(ExitCode::UsageError);
        }
        console.write_out(help_text(help->topic, console.out_has_color()));
        return to_int(ExitCode::Success);
    }
    if (std::holds_alternative<VersionRequest>(invocation)) {
        console.write_out(L"isoforge " + std::wstring(version()) + L"\n");
        return to_int(ExitCode::Success);
    }
    if (const auto* list = std::get_if<ListOptions>(&invocation)) {
        return run_list(*list, console, services);
    }
    if (const auto* info = std::get_if<InfoOptions>(&invocation)) {
        return run_info(*info, console, services);
    }
    if (const auto* create = std::get_if<CreateOptions>(&invocation)) {
        return run_create(*create, console, services, command_line(args));
    }
    return run_verify(std::get<VerifyOptions>(invocation), console, services);
}

}  // namespace

int run(std::span<const std::wstring_view> args, Console& console) {
    const InterruptHandler interrupt;
    SystemDriveProvider drives;
    util::Win32FileSystem files;
    return run(args, console, Services{drives, files, [] { return InterruptHandler::requested(); }});
}

int run(std::span<const std::wstring_view> args, Console& console, const Services& services) {
    const ParseResult result = parse_command_line(args);
    if (!result.invocation) {
        write_usage_error(console, result.error, result.command, result.missing_required);
        return to_int(ExitCode::UsageError);
    }

    // No exception may escape: an uncaught exception would end the process with a code that
    // collides with the documented exit codes.
    try {
        return dispatch(*result.invocation, console, services, args);
    } catch (const device::DeviceError& error) {
        write_error(console, describe(error));
        return to_int(exit_code_for(error));
    } catch (const util::FileError& error) {
        write_error(console, error.message());
        return to_int(ExitCode::OutputError);
    } catch (const std::exception& error) {
        write_error(console, L"internal error: " + util::widen_ascii(error.what()));
        return to_int(ExitCode::OtherError);
    } catch (...) {
        write_error(console, L"internal error");
        return to_int(ExitCode::OtherError);
    }
}

}  // namespace isoforge::cli
