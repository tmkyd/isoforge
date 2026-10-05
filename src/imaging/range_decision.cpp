// SPDX-License-Identifier: Apache-2.0

#include "imaging/range_decision.h"

#include <algorithm>
#include <limits>

#include "device/scsi_device.h"
#include "util/strings.h"

namespace isoforge::imaging {
namespace {

// Runs one file system probe; a sector the drive cannot read becomes a reason and an Invalid
// result. Windows errors (DeviceError) end the command instead.
fs::FsProbe run_probe(fs::FsProbe (*probe)(const fs::SectorReader&, std::uint32_t), const fs::SectorReader& read,
                      std::uint32_t sectors, const wchar_t* name, std::vector<std::wstring>& reasons) {
    try {
        fs::FsProbe result = probe(read, sectors);
        if (result.status == fs::FsProbe::Status::Invalid) {
            reasons.push_back(result.detail);
        }
        return result;
    } catch (const device::ScsiError& error) {
        std::wstring detail = std::wstring(L"could not read the ") + name + L" descriptors: " + util::widen_ascii(error.what());
        reasons.push_back(detail);
        return fs::FsProbe{fs::FsProbe::Status::Invalid, 0, detail};
    }
}

std::optional<SectorRange> media_range(const media::MediaInfo& info, std::vector<std::wstring>& reasons) {
    const media::TrackInformation* track = media::data_track(info);
    if (!info.capacity || track == nullptr) {
        reasons.push_back(L"the capacity or the track information is missing");
        return std::nullopt;
    }
    const std::uint64_t capacity = std::uint64_t{info.capacity->last_lba} + 1;
    if (capacity > std::numeric_limits<std::uint32_t>::max()) {
        reasons.push_back(L"the capacity reported by the drive is too large");
        return std::nullopt;
    }
    const std::uint64_t size = track->size;
    const std::uint64_t difference = capacity > size ? capacity - size : size - capacity;
    // CDs written track at once may differ by the run-out sectors at the end of the track; the
    // larger value is used and unreadable last sectors fall under the CD tail exception.
    const bool cd = info.profile && info.profile->family == media::MediaFamily::Cd;
    if (track->start != 0 || (difference != 0 && !(cd && difference <= kCdCapacityTolerance))) {
        reasons.push_back(L"the capacity (" + std::to_wstring(capacity) +
                          L" sectors) does not match the track (start " + std::to_wstring(track->start) + L", " +
                          std::to_wstring(track->size) + L" sectors)");
        return std::nullopt;
    }
    return SectorRange{0, static_cast<std::uint32_t>(std::max(capacity, size))};
}

}  // namespace

RangeDecision decide_range(const media::MediaInfo& info, const fs::SectorReader& read) {
    RangeDecision decision;
    decision.media = media_range(info, decision.reasons);
    if (!decision.media) {
        return decision;
    }
    const std::uint32_t sectors = decision.media->count;

    decision.iso9660 = run_probe(&fs::probe_iso9660, read, sectors, L"ISO 9660", decision.reasons);
    decision.udf = run_probe(&fs::probe_udf, read, sectors, L"UDF", decision.reasons);
    if (!decision.reasons.empty()) {
        return decision;
    }

    for (const fs::FsProbe* probe : {&decision.iso9660, &decision.udf}) {
        if (probe->status == fs::FsProbe::Status::Found) {
            decision.filesystem_end = std::max(decision.filesystem_end.value_or(0), probe->end_sector);
        }
    }
    if (!decision.filesystem_end) {
        decision.reasons.push_back(L"no ISO 9660 or UDF file system was found");
        return decision;
    }
    if (*decision.filesystem_end > decision.media->end()) {
        decision.reasons.push_back(L"the file system (" + std::to_wstring(*decision.filesystem_end) +
                                   L" sectors) extends beyond the end of the disc (" +
                                   std::to_wstring(decision.media->end()) + L" sectors)");
        return decision;
    }
    decision.adopted = decision.media;
    return decision;
}

}  // namespace isoforge::imaging
