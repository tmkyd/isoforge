// SPDX-License-Identifier: Apache-2.0

#include "media/responses.h"

#include "device/scsi_device.h"
#include "util/byte_order.h"

namespace isoforge::media {
namespace {

using util::be16;
using util::be32;

void require_size(std::span<const std::uint8_t> response, std::size_t minimum, const char* what) {
    if (response.size() < minimum) {
        throw device::ResponseError(std::string(what) + " response is too short (" + std::to_string(response.size()) +
                            " bytes, at least " + std::to_string(minimum) + " required)");
    }
}

// Bytes actually covered by a 2-byte length field at offset 0.
std::size_t declared_size16(std::span<const std::uint8_t> response, const char* what) {
    require_size(response, 2, what);
    const std::size_t declared = std::size_t{2} + be16(response, 0);
    if (declared > response.size()) {
        throw device::ResponseError(std::string(what) + " response declares " + std::to_string(declared) +
                            " bytes but has " + std::to_string(response.size()));
    }
    return declared;
}

}  // namespace

std::uint16_t parse_current_profile(std::span<const std::uint8_t> response) {
    require_size(response, 8, "GET CONFIGURATION");
    return be16(response, 6);
}

DiscInformation parse_disc_information(std::span<const std::uint8_t> response) {
    const std::size_t size = declared_size16(response, "READ DISC INFORMATION");
    require_size(response.first(size), 12, "READ DISC INFORMATION");
    DiscInformation info;
    info.last_session_state = static_cast<DiscInformation::SessionState>((response[2] >> 2) & 0x03);
    info.disc_status = static_cast<DiscInformation::DiscStatus>(response[2] & 0x03);
    info.first_track = response[3];
    info.sessions = static_cast<std::uint16_t>(response[4] | (response[9] << 8));
    info.first_track_in_last_session = static_cast<std::uint16_t>(response[5] | (response[10] << 8));
    info.last_track_in_last_session = static_cast<std::uint16_t>(response[6] | (response[11] << 8));
    return info;
}

Toc parse_toc(std::span<const std::uint8_t> response) {
    const std::size_t size = declared_size16(response, "READ TOC");
    require_size(response.first(size), 4, "READ TOC");
    if ((size - 4) % 8 != 0) {
        throw device::ResponseError("READ TOC response length " + std::to_string(size) +
                            " is not a whole number of track descriptors");
    }
    Toc toc;
    for (std::size_t offset = 4; offset < size; offset += 8) {
        TocEntry entry;
        entry.control = static_cast<std::uint8_t>(response[offset + 1] & 0x0F);
        entry.track = response[offset + 2];
        toc.entries.push_back(entry);
    }
    return toc;
}

TrackInformation parse_track_information(std::span<const std::uint8_t> response) {
    const std::size_t size = declared_size16(response, "READ TRACK INFORMATION");
    const std::span<const std::uint8_t> data = response.first(size);
    require_size(data, 28, "READ TRACK INFORMATION");
    TrackInformation info;
    info.track = data[2];
    info.session = data[3];
    info.track_mode = static_cast<std::uint8_t>(data[5] & 0x0F);
    info.blank = (data[6] & 0x40) != 0;
    info.packet = (data[6] & 0x20) != 0;
    info.data_mode = static_cast<std::uint8_t>(data[6] & 0x0F);
    info.start = be32(data, 8);
    info.size = be32(data, 24);
    if (data.size() >= 34) {
        info.track = static_cast<std::uint16_t>(info.track | (data[32] << 8));
        info.session = static_cast<std::uint16_t>(info.session | (data[33] << 8));
    }
    return info;
}

}  // namespace isoforge::media
