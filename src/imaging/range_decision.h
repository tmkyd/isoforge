// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "device/scsi_commands.h"
#include "fs/filesystem.h"
#include "media/assessment.h"

namespace isoforge::imaging {

struct SectorRange {
    std::uint32_t start = 0;
    std::uint32_t count = 0;

    std::uint64_t end() const noexcept { return std::uint64_t{start} + count; }
    std::uint64_t bytes() const noexcept { return std::uint64_t{count} * device::kSectorSize; }
    bool operator==(const SectorRange&) const = default;
};

// The range to save and how it was decided.
struct RangeDecision {
    std::optional<SectorRange> media;      // From READ CAPACITY and the track information.
    fs::FsProbe iso9660;
    fs::FsProbe udf;
    std::optional<std::uint64_t> filesystem_end;
    std::optional<SectorRange> adopted;    // Set only when the range is determined.
    std::vector<std::wstring> reasons;     // Why the range could not be determined.
};

// Largest difference between READ CAPACITY and the track size accepted on CDs.
inline constexpr std::uint64_t kCdCapacityTolerance = 2;

// Decides the range for a disc that passed media::assess(). The media-side range requires READ
// CAPACITY to agree with the single data track starting at 0; on CDs a difference of up to
// kCdCapacityTolerance sectors is accepted and the larger value is used. File system descriptors
// are read through `read`; a read failure leaves the range undetermined.
RangeDecision decide_range(const media::MediaInfo& info, const fs::SectorReader& read);

}  // namespace isoforge::imaging
