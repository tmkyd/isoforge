// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "device/scsi_device.h"
#include "media/profile.h"
#include "media/responses.h"

namespace isoforge::media {

// Information collected from the drive for the support judgment.
struct MediaInfo {
    std::optional<ProfileInfo> profile;
    std::optional<device::ReadCapacityData> capacity;
    std::optional<DiscInformation> disc;
    std::optional<Toc> toc;                 // CD only.
    std::vector<TrackInformation> tracks;   // Tracks of the last session.
    std::vector<std::wstring> errors;       // Commands that failed; any error makes the disc unsupported.
};

// Issues GET CONFIGURATION, READ CAPACITY, READ DISC INFORMATION, READ TOC (CD) and
// READ TRACK INFORMATION. SCSI command failures and malformed responses are recorded in
// MediaInfo::errors; collection stops at the first failure. Device (Windows API) errors propagate.
MediaInfo collect_media_info(device::ScsiDevice& device);

// The current profile (GET CONFIGURATION). Throws device::ScsiError when it cannot be read.
ProfileInfo read_current_profile(device::ScsiDevice& device);

// A disc that ends with an empty session after closed ones (disc status incomplete, last session
// empty). isoforge accepts it when exactly one session is closed.
bool is_appendable(const DiscInformation& disc) noexcept;

// Track numbers that hold the data: the last session of a closed disc, or the tracks before the
// empty session of an appendable disc. std::nullopt when the numbers are inconsistent.
struct TrackSpan {
    std::uint16_t first = 0;
    std::uint16_t last = 0;
};
std::optional<TrackSpan> data_track_span(const DiscInformation& disc) noexcept;

// The single data track to save, or nullptr when there is not exactly one with track information.
const TrackInformation* data_track(const MediaInfo& info) noexcept;

struct Assessment {
    bool supported = false;
    std::vector<std::wstring> reasons;  // Why the disc is not supported; empty when supported.
};

// Judges from the collected information whether the disc is supported.
Assessment assess(const MediaInfo& info);

}  // namespace isoforge::media
