// SPDX-License-Identifier: Apache-2.0

#include "media/assessment.h"

#include "util/strings.h"

namespace isoforge::media {
namespace {

// The highest track number on a CD; also bounds READ TRACK INFORMATION on other media.
constexpr std::uint16_t kMaxTracksToRead = 99;

using Disc = DiscInformation;

// The session count and the disc status.
void check_sessions(const Disc& disc, const ProfileInfo& profile, std::vector<std::wstring>& reasons) {
    const bool appendable = is_appendable(disc);
    // An appendable disc also counts its empty session (MMC), which holds no data.
    const unsigned closed_sessions = appendable ? (disc.sessions > 0 ? disc.sessions - 1u : 0u) : disc.sessions;
    if (closed_sessions == 0 && disc.disc_status != Disc::DiscStatus::Empty) {
        reasons.push_back(L"the disc has no closed session");
    } else if (closed_sessions > 1) {
        reasons.push_back(L"multi-session discs are not supported (" + std::to_wstring(closed_sessions) +
                          L" sessions)");
    }
    switch (disc.disc_status) {
    case Disc::DiscStatus::Empty:
        reasons.push_back(L"the disc is blank");
        break;
    case Disc::DiscStatus::Incomplete:
        if (!appendable) {
            reasons.push_back(L"the disc is not closed and its last session is not empty (it may be being written)");
        }
        break;
    case Disc::DiscStatus::Complete:
        if (disc.last_session_state != Disc::SessionState::Complete) {
            reasons.push_back(L"the last session is not closed");
        }
        break;
    case Disc::DiscStatus::Other:
        if (!profile.overwritable) {
            reasons.push_back(L"the disc status is not complete");
        }
        break;
    }
}

// One non-blank data track. Returns false when the track numbers are inconsistent, which leaves
// nothing more to check.
bool check_tracks(const MediaInfo& info, std::vector<std::wstring>& reasons) {
    const std::optional<TrackSpan> span = data_track_span(*info.disc);
    if (!span) {
        reasons.push_back(L"the track numbers reported by the drive are inconsistent");
        return false;
    }
    if (span->last != span->first) {
        reasons.push_back(L"discs with more than one track are not supported (" +
                          std::to_wstring(span->last - span->first + 1) + L" tracks)");
    }
    if (info.tracks.empty()) {
        reasons.push_back(L"the track information is missing");
    }
    for (const TrackInformation& track : info.tracks) {
        if (track.blank) {
            reasons.push_back(L"track " + std::to_wstring(track.track) + L" is blank");
        }
    }
    return true;
}

// The TOC and the track modes of a CD: one Mode 1 data track, not packet-written.
void check_cd(const MediaInfo& info, std::vector<std::wstring>& reasons) {
    if (!info.toc) {
        reasons.push_back(L"the TOC is missing");
    } else {
        int data_tracks = 0;
        int audio_tracks = 0;
        for (const TocEntry& entry : info.toc->entries) {
            if (!entry.is_lead_out()) {
                (entry.is_data() ? data_tracks : audio_tracks) += 1;
            }
        }
        if (audio_tracks > 0) {
            reasons.push_back(L"audio tracks are not supported (" + std::to_wstring(audio_tracks) + L" audio tracks)");
        }
        if (data_tracks > 1) {
            reasons.push_back(L"CDs with more than one data track are not supported");
        }
        if (data_tracks == 0 && audio_tracks == 0) {
            reasons.push_back(L"the TOC has no tracks");
        }
    }
    for (const TrackInformation& track : info.tracks) {
        if ((track.track_mode & 0x04) == 0) {
            reasons.push_back(L"track " + std::to_wstring(track.track) + L" is not a data track");
        } else if (track.data_mode == 2) {
            reasons.push_back(L"CD Mode 2 (CD-ROM XA) is not supported");
        } else if (track.data_mode != 1) {
            reasons.push_back(L"the data mode of track " + std::to_wstring(track.track) + L" is unknown");
        }
        if (track.packet) {
            reasons.push_back(L"packet-written CDs are not supported");
        }
    }
}

}  // namespace

bool is_appendable(const DiscInformation& disc) noexcept {
    return disc.disc_status == Disc::DiscStatus::Incomplete && disc.last_session_state == Disc::SessionState::Empty;
}

std::optional<TrackSpan> data_track_span(const DiscInformation& disc) noexcept {
    if (is_appendable(disc)) {
        // The tracks before the empty session that an appendable disc ends with.
        if (disc.first_track == 0 || disc.first_track_in_last_session <= disc.first_track) {
            return std::nullopt;
        }
        return TrackSpan{disc.first_track, static_cast<std::uint16_t>(disc.first_track_in_last_session - 1)};
    }
    if (disc.first_track_in_last_session == 0 || disc.last_track_in_last_session < disc.first_track_in_last_session) {
        return std::nullopt;
    }
    return TrackSpan{disc.first_track_in_last_session, disc.last_track_in_last_session};
}

const TrackInformation* data_track(const MediaInfo& info) noexcept {
    if (!info.disc) {
        return nullptr;
    }
    const std::optional<TrackSpan> span = data_track_span(*info.disc);
    if (!span || span->first != span->last) {
        return nullptr;
    }
    for (const TrackInformation& track : info.tracks) {
        if (track.track == span->first) {
            return &track;
        }
    }
    return nullptr;
}

ProfileInfo read_current_profile(device::ScsiDevice& device) {
    // RT = 2, starting feature 0: the feature header and the Profile List feature only.
    return profile_info(parse_current_profile(device.get_configuration(2, 0)));
}

MediaInfo collect_media_info(device::ScsiDevice& device) {
    MediaInfo info;
    // Records a failed step and tells the caller to stop collecting.
    const auto record = [&](const wchar_t* what, const std::exception& error) {
        info.errors.push_back(std::wstring(L"could not read ") + what + L": " + util::widen_ascii(error.what()));
    };

    try {
        info.profile = read_current_profile(device);
    } catch (const device::ScsiError& e) {
        record(L"the disc type", e);
        return info;
    }
    if (info.profile->family == MediaFamily::None || !info.profile->supported) {
        return info;
    }

    const auto step = [&](const wchar_t* what, const auto& action) {
        try {
            action();
            return true;
        } catch (const device::ScsiError& e) {
            record(what, e);
        }
        return false;
    };

    if (!step(L"the capacity", [&] { info.capacity = device.read_capacity(); })) {
        return info;
    }
    if (!step(L"the disc information", [&] { info.disc = parse_disc_information(device.read_disc_information()); })) {
        return info;
    }
    if (info.profile->family == MediaFamily::Cd &&
        !step(L"the TOC", [&] { info.toc = parse_toc(device.read_toc(0, false, 0)); })) {
        return info;
    }

    const std::optional<TrackSpan> span = data_track_span(*info.disc);
    if (!span || span->last - span->first >= kMaxTracksToRead) {
        return info;  // assess() reports the inconsistent track numbers.
    }
    for (std::uint32_t track = span->first; track <= span->last; ++track) {
        if (!step(L"the track information", [&] {
                info.tracks.push_back(parse_track_information(device.read_track_information(1, track)));
            })) {
            return info;
        }
    }
    return info;
}

Assessment assess(const MediaInfo& info) {
    Assessment result;
    std::vector<std::wstring>& reasons = result.reasons;

    if (!info.errors.empty()) {
        reasons = info.errors;
        return result;
    }
    if (!info.profile) {
        reasons.push_back(L"the disc type is unknown");
        return result;
    }
    if (!info.profile->supported) {
        reasons.push_back(info.profile->reason);
        return result;
    }
    if (!info.capacity || !info.disc) {
        reasons.push_back(L"the disc information is incomplete");
        return result;
    }

    if (info.capacity->block_length != device::kSectorSize) {
        reasons.push_back(L"the logical block length is " + std::to_wstring(info.capacity->block_length) +
                          L" bytes, not 2048 bytes");
    }
    check_sessions(*info.disc, *info.profile, reasons);
    if (!check_tracks(info, reasons)) {
        return result;
    }
    if (info.profile->family == MediaFamily::Cd) {
        check_cd(info, reasons);
    }

    result.supported = reasons.empty();
    return result;
}

}  // namespace isoforge::media
