// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "cli/command_line.h"
#include "cli/console.h"
#include "cli/services.h"
#include "device/scsi_device.h"
#include "imaging/range_decision.h"
#include "media/assessment.h"

namespace isoforge::cli {

// Everything info shows.
struct InfoReport {
    std::wstring device_path;
    std::optional<std::uint32_t> device_number;
    std::wstring vendor;
    std::wstring product;
    std::wstring revision;
    media::MediaInfo media;
    media::Assessment assessment;
    std::optional<imaging::RangeDecision> range;  // Only for discs that pass the assessment.

    bool supported() const { return assessment.supported && range && range->adopted; }
    // Reasons from the assessment and the range decision.
    std::vector<std::wstring> reasons() const;
};

std::wstring format_info_text(const InfoReport& report);

// Waits up to `wait_seconds` for a disc. Returns std::nullopt once a disc is ready, otherwise why
// not, e.g. "\\.\Z:: no disc". Throws Interrupted on Ctrl+C.
std::optional<std::wstring> wait_for_disc(device::ScsiDevice& scsi, const std::wstring& device_path,
                                          const Services& services, std::uint32_t wait_seconds);

// Reads the disc details, judges the disc and, when it is supported, decides the range.
InfoReport inspect_disc(device::ScsiDevice& scsi, const OpenedDrive& drive);

// isoforge info: waits for the disc, judges it and decides the range. Exit code 0 when the disc is
// supported and the range is determined, 2 otherwise. Ctrl+C during the wait ends the command with
// exit code 130.
int run_info(const InfoOptions& options, Console& console, const Services& services);

}  // namespace isoforge::cli
