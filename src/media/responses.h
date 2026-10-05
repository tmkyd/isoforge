// SPDX-License-Identifier: Apache-2.0
//
// Parsers for MMC command responses. They throw device::ResponseError when a response is too
// short or inconsistent; they never guess missing values.

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "device/scsi_device.h"

namespace isoforge::media {

// GET CONFIGURATION: Current Profile from the feature header.
std::uint16_t parse_current_profile(std::span<const std::uint8_t> response);

// READ DISC INFORMATION, data type 000b.
struct DiscInformation {
    enum class DiscStatus : std::uint8_t { Empty = 0, Incomplete = 1, Complete = 2, Other = 3 };
    enum class SessionState : std::uint8_t { Empty = 0, Incomplete = 1, Reserved = 2, Complete = 3 };

    DiscStatus disc_status = DiscStatus::Empty;
    SessionState last_session_state = SessionState::Empty;
    std::uint16_t first_track = 0;               // First track number on the disc.
    std::uint16_t sessions = 0;
    std::uint16_t first_track_in_last_session = 0;
    std::uint16_t last_track_in_last_session = 0;
};
DiscInformation parse_disc_information(std::span<const std::uint8_t> response);

// READ TOC/PMA/ATIP, format 0000b (formatted TOC) with LBA addresses.
struct TocEntry {
    std::uint8_t control = 0;  // Bit 2 set: data track.
    std::uint8_t track = 0;    // AAh for the lead-out.

    bool is_data() const noexcept { return (control & 0x04) != 0; }
    bool is_lead_out() const noexcept { return track == 0xAA; }
};
struct Toc {
    std::vector<TocEntry> entries;  // Including the lead-out.
};
Toc parse_toc(std::span<const std::uint8_t> response);

// READ TRACK INFORMATION.
struct TrackInformation {
    std::uint16_t track = 0;
    std::uint16_t session = 0;
    std::uint8_t track_mode = 0;  // CONTROL nibble; bit 2 set: data track (CD).
    std::uint8_t data_mode = 0;   // 1: Mode 1, 2: Mode 2, 0Fh: unknown.
    bool blank = false;
    bool packet = false;
    std::uint32_t start = 0;
    std::uint32_t size = 0;
};
TrackInformation parse_track_information(std::span<const std::uint8_t> response);

}  // namespace isoforge::media
