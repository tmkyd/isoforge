// SPDX-License-Identifier: Apache-2.0

#include "cli/info_command.h"

#include <string_view>

#include "cli/exit_code.h"
#include "cli/json_values.h"
#include "cli/reporting.h"
#include "cli/style.h"
#include "device/scsi_device.h"
#include "util/json.h"
#include "util/strings.h"

namespace isoforge::cli {
namespace {

using media::DiscInformation;

std::wstring disc_status_name(DiscInformation::DiscStatus status) {
    switch (status) {
    case DiscInformation::DiscStatus::Empty:
        return L"empty";
    case DiscInformation::DiscStatus::Incomplete:
        return L"incomplete";
    case DiscInformation::DiscStatus::Complete:
        return L"complete";
    case DiscInformation::DiscStatus::Other:
        break;
    }
    return L"other";
}

std::wstring session_state_name(DiscInformation::SessionState state) {
    switch (state) {
    case DiscInformation::SessionState::Empty:
        return L"empty";
    case DiscInformation::SessionState::Incomplete:
        return L"incomplete";
    case DiscInformation::SessionState::Reserved:
        return L"reserved";
    case DiscInformation::SessionState::Complete:
        break;
    }
    return L"complete";
}

std::wstring probe_text(const fs::FsProbe& probe) {
    switch (probe.status) {
    case fs::FsProbe::Status::Absent:
        return L"not found";
    case fs::FsProbe::Status::Found:
        return std::to_wstring(probe.end_sector) + L" sectors";
    case fs::FsProbe::Status::Invalid:
        break;
    }
    return L"invalid (" + probe.detail + L")";
}

std::wstring probe_json(const fs::FsProbe& probe) {
    switch (probe.status) {
    case fs::FsProbe::Status::Absent:
        return L"{\"status\": \"absent\", \"sectors\": null}";
    case fs::FsProbe::Status::Found:
        return L"{\"status\": \"found\", \"sectors\": " + std::to_wstring(probe.end_sector) + L"}";
    case fs::FsProbe::Status::Invalid:
        break;
    }
    return L"{\"status\": \"invalid\", \"sectors\": null, \"detail\": " + util::json_string(probe.detail) + L"}";
}

void line(std::wstring& text, std::wstring_view label, const std::wstring& value) {
    text += labeled(label, value, 16) + L"\n";
}

std::wstring device_line(const InfoReport& report) {
    std::wstring value = report.device_path;
    if (report.device_number) {
        value += L" (CdRom" + std::to_wstring(*report.device_number) + L")";
    }
    return value;
}

}  // namespace

std::vector<std::wstring> InfoReport::reasons() const {
    std::vector<std::wstring> all = assessment.reasons;
    if (range) {
        all.insert(all.end(), range->reasons.begin(), range->reasons.end());
    }
    return all;
}

std::wstring format_info_text(const InfoReport& report) {
    std::wstring text;
    line(text, L"Device:", device_line(report));
    line(text, L"Model:", format_model(report.vendor, report.product, report.revision));
    const media::MediaInfo& m = report.media;
    if (m.profile) {
        line(text, L"Disc type:",
             m.profile->name + L" (profile " + util::widen_ascii(util::hex_with_suffix(m.profile->profile, 4)) + L")");
    }
    if (m.capacity) {
        line(text, L"Capacity:",
             std::to_wstring(std::uint64_t{m.capacity->last_lba} + 1) + L" blocks of " +
                 std::to_wstring(m.capacity->block_length) + L" bytes");
    }
    if (m.disc) {
        line(text, L"Disc status:",
             disc_status_name(m.disc->disc_status) + L", last session " +
                 session_state_name(m.disc->last_session_state) + L", " + std::to_wstring(m.disc->sessions) +
                 L" session(s), tracks " + std::to_wstring(m.disc->first_track_in_last_session) + L"-" +
                 std::to_wstring(m.disc->last_track_in_last_session));
    }
    for (const media::TrackInformation& track : m.tracks) {
        std::wstring value = L"start " + std::to_wstring(track.start) + L", " + std::to_wstring(track.size) +
                             L" sectors, session " + std::to_wstring(track.session);
        if (m.profile && m.profile->family == media::MediaFamily::Cd) {
            value += (track.track_mode & 0x04) != 0 ? L", data" : L", audio";
            value += L", data mode " + std::to_wstring(track.data_mode);
        }
        if (track.blank) {
            value += L", blank";
        }
        if (track.packet) {
            value += L", packet";
        }
        line(text, L"Track " + std::to_wstring(track.track) + L":", value);
    }
    if (report.range) {
        const imaging::RangeDecision& r = *report.range;
        if (r.media) {
            line(text, L"Media range:", format_range(*r.media));
        }
        line(text, L"ISO 9660:", probe_text(r.iso9660));
        line(text, L"UDF:", probe_text(r.udf));
        if (r.adopted) {
            line(text, L"Adopted range:", format_range(*r.adopted));
        }
    }
    line(text, L"Supported:", report.supported() ? L"yes" : L"no");
    const std::vector<std::wstring> reasons = report.reasons();
    if (!reasons.empty()) {
        text += L"Reasons:\n";
        for (const std::wstring& reason : reasons) {
            text += L"  - " + reason + L"\n";
        }
    }
    return text;
}

namespace {

std::wstring format_info_json(const InfoReport& report) {
    const media::MediaInfo& m = report.media;
    std::wstring text = L"{\n";
    text += L"  \"device\": " + util::json_string(report.device_path) + L",\n";
    text += L"  \"device_number\": " +
            (report.device_number ? std::to_wstring(*report.device_number) : std::wstring(L"null")) + L",\n";
    text += L"  \"vendor\": " + util::json_string(report.vendor) + L",\n";
    text += L"  \"product\": " + util::json_string(report.product) + L",\n";
    text += L"  \"revision\": " + util::json_string(report.revision) + L",\n";
    text += L"  \"profile\": " + profile_json(m.profile) + L",\n";
    text += L"  \"capacity\": ";
    text += m.capacity ? L"{\"blocks\": " + std::to_wstring(std::uint64_t{m.capacity->last_lba} + 1) +
                             L", \"block_length\": " + std::to_wstring(m.capacity->block_length) + L"}"
                       : L"null";
    text += L",\n  \"disc\": ";
    text += m.disc ? L"{\"status\": " + util::json_string(disc_status_name(m.disc->disc_status)) +
                         L", \"last_session_state\": " +
                         util::json_string(session_state_name(m.disc->last_session_state)) + L", \"sessions\": " +
                         std::to_wstring(m.disc->sessions) + L", \"first_track\": " +
                         std::to_wstring(m.disc->first_track_in_last_session) + L", \"last_track\": " +
                         std::to_wstring(m.disc->last_track_in_last_session) + L"}"
                   : L"null";
    text += L",\n  \"tracks\": [";
    for (std::size_t i = 0; i < m.tracks.size(); ++i) {
        const media::TrackInformation& t = m.tracks[i];
        text += i == 0 ? L"" : L", ";
        text += L"{\"number\": " + std::to_wstring(t.track) + L", \"session\": " + std::to_wstring(t.session) +
                L", \"start\": " + std::to_wstring(t.start) + L", \"sectors\": " + std::to_wstring(t.size) +
                L", \"track_mode\": " + std::to_wstring(t.track_mode) + L", \"data_mode\": " +
                std::to_wstring(t.data_mode) + L", \"blank\": " + (t.blank ? L"true" : L"false") +
                L", \"packet\": " + (t.packet ? L"true" : L"false") + L"}";
    }
    text += L"],\n";
    if (report.range) {
        text += L"  \"media_range\": " + range_json(report.range->media) + L",\n";
        text += L"  \"iso9660\": " + probe_json(report.range->iso9660) + L",\n";
        text += L"  \"udf\": " + probe_json(report.range->udf) + L",\n";
        text += L"  \"adopted_range\": " + range_json(report.range->adopted) + L",\n";
    } else {
        text += L"  \"media_range\": null,\n  \"iso9660\": null,\n  \"udf\": null,\n  \"adopted_range\": null,\n";
    }
    text += L"  \"supported\": ";
    text += report.supported() ? L"true" : L"false";
    text += L",\n  \"reasons\": [";
    const std::vector<std::wstring> reasons = report.reasons();
    for (std::size_t i = 0; i < reasons.size(); ++i) {
        text += (i == 0 ? L"" : L", ") + util::json_string(reasons[i]);
    }
    text += L"]\n}\n";
    return text;
}

}  // namespace

std::optional<std::wstring> wait_for_disc(device::ScsiDevice& scsi, const std::wstring& device_path,
                                          const Services& services, std::uint32_t wait_seconds) {
    const media::MediaStatus status = media::wait_until_ready(
        scsi,
        [&](std::chrono::milliseconds d) {
            if (services.interrupted && services.interrupted()) {
                throw Interrupted{};
            }
            services.drives.sleep(d);
        },
        std::chrono::seconds(wait_seconds));
    const auto sense = [&] { return util::widen_ascii(device::describe(status.sense)); };
    switch (status.state) {
    case media::MediaState::Loaded:
        return std::nullopt;
    case media::MediaState::NoDisc:
        return device_path + L": no disc";
    case media::MediaState::NotReady:
        return device_path + L": the drive did not become ready within " + std::to_wstring(wait_seconds) +
               L" seconds (" + sense() + L")";
    default:
        return device_path + L": the drive is not ready (" + sense() + L")";
    }
}

InfoReport inspect_disc(device::ScsiDevice& scsi, const OpenedDrive& drive) {
    InfoReport report;
    report.device_path = drive.device_path;
    report.device_number = drive.device_number;
    report.vendor = drive.vendor;
    report.product = drive.product;
    report.revision = drive.revision;
    report.media = media::collect_media_info(scsi);
    report.assessment = media::assess(report.media);
    if (report.assessment.supported) {
        report.range = imaging::decide_range(report.media, [&](std::uint32_t lba, std::span<std::uint8_t> out) {
            scsi.read10(lba, 1, out);
        });
    }
    return report;
}

int run_info(const InfoOptions& options, Console& console, const Services& services) {
    OpenedDrive drive = services.drives.open(options.drive);
    device::ScsiDevice scsi(*drive.transport);
    try {
        if (const auto problem = wait_for_disc(scsi, drive.device_path, services, options.wait_seconds)) {
            write_error(console, *problem);
            return to_int(ExitCode::Unsupported);
        }
    } catch (const Interrupted&) {
        write_error(console, L"interrupted");
        return to_int(ExitCode::Interrupted);
    }

    const InfoReport report = inspect_disc(scsi, drive);
    console.write_out(options.json ? format_info_json(report) : format_info_text(report));
    return to_int(report.supported() ? ExitCode::Success : ExitCode::Unsupported);
}

}  // namespace isoforge::cli
