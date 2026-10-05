// SPDX-License-Identifier: Apache-2.0

#include "cli/list_command.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "cli/device_errors.h"
#include "cli/exit_code.h"
#include "cli/json_values.h"
#include "cli/style.h"
#include "device/drive_path.h"
#include "util/json.h"
#include "util/strings.h"

namespace isoforge::cli {
namespace {

std::wstring letter_text(const device::DriveEntry& entry) {
    return entry.letter ? std::wstring{*entry.letter, L':'} : std::wstring(L"-");
}

std::wstring device_text(const device::DriveEntry& entry) {
    return device::DrivePath{device::DrivePath::Kind::CdRom, L'\0', entry.number}.device_path();
}

std::wstring media_text(const DriveReport& report) {
    if (report.error) {
        return L"error";
    }
    return report.media ? media::describe(report.media->state) : L"unknown";
}

std::wstring media_json_value(const DriveReport& report) {
    if (report.error || !report.media) {
        return L"null";
    }
    switch (report.media->state) {
    case media::MediaState::Loaded:
        return L"\"loaded\"";
    case media::MediaState::NoDisc:
        return L"\"none\"";
    case media::MediaState::NotReady:
        return L"\"not_ready\"";
    case media::MediaState::Unknown:
        break;
    }
    return L"\"unknown\"";
}

std::wstring format_list_text(const std::vector<DriveReport>& reports) {
    if (reports.empty()) {
        return L"No optical drives found.\n";
    }
    const std::array<std::wstring, 7> header = {L"Drive",    L"Device", L"Vendor", L"Product",
                                                L"Revision", L"Media",  L"Disc"};
    std::vector<std::array<std::wstring, 7>> rows = {header};
    for (const DriveReport& report : reports) {
        rows.push_back({letter_text(report.entry), device_text(report.entry), report.vendor, report.product,
                        report.revision, media_text(report), report.profile ? report.profile->name : L"-"});
    }
    std::array<std::size_t, 7> widths{};
    for (const auto& row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }
    std::wstring text;
    for (const auto& row : rows) {
        std::wstring line;
        for (std::size_t i = 0; i < row.size(); ++i) {
            line += row[i];
            if (i + 1 < row.size()) {
                line.append(widths[i] - row[i].size() + 2, L' ');
            }
        }
        text += line + L"\n";
    }
    return text;
}

std::wstring format_list_json(const std::vector<DriveReport>& reports) {
    std::wstring text = L"{\n  \"drives\": [";
    for (std::size_t i = 0; i < reports.size(); ++i) {
        const DriveReport& r = reports[i];
        text += i == 0 ? L"\n" : L",\n";
        text += L"    {\"drive_letter\": ";
        text += r.entry.letter ? util::json_string(letter_text(r.entry)) : L"null";
        text += L", \"device\": " + util::json_string(device_text(r.entry));
        text += L", \"vendor\": " + util::json_string(r.vendor);
        text += L", \"product\": " + util::json_string(r.product);
        text += L", \"revision\": " + util::json_string(r.revision);
        text += L", \"media\": " + media_json_value(r);
        text += L", \"profile\": " + profile_json(r.profile);
        text += L", \"error\": ";
        text += r.error ? util::json_string(util::widen_ascii(r.error->what())) : L"null";
        text += L"}";
    }
    text += reports.empty() ? L"]\n}\n" : L"\n  ]\n}\n";
    return text;
}

}  // namespace

int run_list(const ListOptions& options, Console& console, const Services& services) {
    std::vector<DriveReport> reports;
    for (const device::DriveEntry& entry : services.drives.enumerate()) {
        if (services.interrupted && services.interrupted()) {
            write_error(console, L"interrupted");
            return to_int(ExitCode::Interrupted);
        }
        reports.push_back(services.drives.probe(entry));
    }

    console.write_out(options.json ? format_list_json(reports) : format_list_text(reports));

    // Other drives are still listed when one fails; the first failure decides the exit code.
    std::optional<ExitCode> code;
    for (const DriveReport& report : reports) {
        if (report.error) {
            write_error(console, describe(*report.error));
            if (!code) {
                code = exit_code_for(*report.error);
            }
        }
    }
    return to_int(code.value_or(ExitCode::Success));
}

}  // namespace isoforge::cli
