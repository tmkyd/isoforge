// SPDX-License-Identifier: Apache-2.0

#include "device/scsi_sense.h"

#include <cstdio>

namespace isoforge::device {
namespace {

const char* sense_key_name(std::uint8_t key) noexcept {
    switch (key) {
    case 0x0: return "NO SENSE";
    case 0x1: return "RECOVERED ERROR";
    case 0x2: return "NOT READY";
    case 0x3: return "MEDIUM ERROR";
    case 0x4: return "HARDWARE ERROR";
    case 0x5: return "ILLEGAL REQUEST";
    case 0x6: return "UNIT ATTENTION";
    case 0x7: return "DATA PROTECT";
    case 0x8: return "BLANK CHECK";
    case 0x9: return "VENDOR SPECIFIC";
    case 0xA: return "COPY ABORTED";
    case 0xB: return "ABORTED COMMAND";
    case 0xD: return "VOLUME OVERFLOW";
    case 0xE: return "MISCOMPARE";
    default: return "RESERVED";
    }
}

}  // namespace

SenseData parse_sense(std::span<const std::uint8_t> raw) noexcept {
    if (raw.empty()) {
        return {};
    }
    const std::uint8_t response_code = raw[0] & 0x7F;
    if (response_code == 0x70 || response_code == 0x71) {
        // Fixed format: key in byte 2, ASC/ASCQ in bytes 12/13 when the additional length covers them.
        if (raw.size() < 3) {
            return {};
        }
        SenseData sense{true, static_cast<std::uint8_t>(raw[2] & 0x0F), 0, 0};
        const std::size_t available = raw.size() >= 8 ? std::size_t{8} + raw[7] : raw.size();
        if (raw.size() >= 14 && available >= 14) {
            sense.asc = raw[12];
            sense.ascq = raw[13];
        }
        return sense;
    }
    if (response_code == 0x72 || response_code == 0x73) {
        if (raw.size() < 4) {
            return {};
        }
        return SenseData{true, static_cast<std::uint8_t>(raw[1] & 0x0F), raw[2], raw[3]};
    }
    return {};
}

std::string describe(const SenseData& sense) {
    if (!sense.valid) {
        return "no valid sense data";
    }
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "sense key %Xh (%s), ASC %02Xh, ASCQ %02Xh", sense.sense_key,
                  sense_key_name(sense.sense_key), sense.asc, sense.ascq);
    return buffer;
}

}  // namespace isoforge::device
