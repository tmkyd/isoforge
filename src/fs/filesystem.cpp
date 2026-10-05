// SPDX-License-Identifier: Apache-2.0

#include "fs/filesystem.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "util/byte_order.h"

namespace isoforge::fs {
namespace {

using util::be16;
using util::be32;
using util::le16;
using util::le32;

constexpr std::uint32_t kSectorSize = 2048;
constexpr std::uint32_t kVolumeRecognitionStart = 16;  // Byte 32768 with 2048-byte sectors.
constexpr std::uint32_t kMaxVolumeRecognitionSectors = 64;
constexpr std::uint32_t kMaxVolumeDescriptorSequenceSectors = 64;

using Sector = std::array<std::uint8_t, kSectorSize>;

std::string_view identifier(const Sector& sector) {
    return std::string_view(reinterpret_cast<const char*>(sector.data()) + 1, 5);
}

bool is_recognition_identifier(std::string_view id) {
    return id == "CD001" || id == "CDW02" || id == "BOOT2" || id == "BEA01" || id == "NSR02" || id == "NSR03" ||
           id == "TEA01";
}

FsProbe invalid(std::wstring detail) {
    return FsProbe{FsProbe::Status::Invalid, 0, std::move(detail)};
}

Sector read_sector(const SectorReader& read, std::uint32_t lba) {
    Sector sector{};
    read(lba, sector);
    return sector;
}

// Volume Recognition Sequence entries from LBA 16 until the first sector without a recognised
// identifier (ECMA-119 and ECMA-167 share this area).
struct RecognitionEntry {
    std::uint32_t lba;
    std::string id;
    std::uint8_t type;
};

std::vector<RecognitionEntry> scan_recognition(const SectorReader& read, std::uint32_t media_sectors) {
    std::vector<RecognitionEntry> entries;
    for (std::uint32_t i = 0; i < kMaxVolumeRecognitionSectors; ++i) {
        const std::uint32_t lba = kVolumeRecognitionStart + i;
        if (lba >= media_sectors) {
            break;
        }
        const Sector sector = read_sector(read, lba);
        const std::string_view id = identifier(sector);
        if (!is_recognition_identifier(id)) {
            break;
        }
        entries.push_back(RecognitionEntry{lba, std::string(id), sector[0]});
    }
    return entries;
}

// Validates an ECMA-167 descriptor tag read at `lba`; returns its identifier.
std::optional<std::uint16_t> valid_tag(const Sector& sector, std::uint32_t lba) {
    std::uint8_t checksum = 0;
    for (std::size_t i = 0; i < 16; ++i) {
        if (i != 4) {
            checksum = static_cast<std::uint8_t>(checksum + sector[i]);
        }
    }
    if (checksum != sector[4]) {
        return std::nullopt;
    }
    const std::uint16_t version = le16(sector, 2);
    if (version != 2 && version != 3) {
        return std::nullopt;
    }
    if (le32(sector, 12) != lba) {
        return std::nullopt;
    }
    const std::uint16_t crc_length = le16(sector, 10);
    if (crc_length > kSectorSize - 16 ||
        udf_crc(std::span<const std::uint8_t>(sector).subspan(16, crc_length)) != le16(sector, 8)) {
        return std::nullopt;
    }
    return le16(sector, 0);
}

constexpr std::uint16_t kTagPrimaryVolume = 1;
constexpr std::uint16_t kTagAnchor = 2;
constexpr std::uint16_t kTagVolumePointer = 3;
constexpr std::uint16_t kTagPartition = 5;
constexpr std::uint16_t kTagTerminating = 8;

struct Extent {
    std::uint32_t length;    // Bytes.
    std::uint32_t location;  // LBA.

    std::uint64_t sectors() const { return (static_cast<std::uint64_t>(length) + kSectorSize - 1) / kSectorSize; }
    std::uint64_t end() const { return location + sectors(); }
};

// Reads a volume descriptor sequence; returns the end of the partitions it describes (0 if none).
std::uint64_t partition_end(const SectorReader& read, const Extent& extent, std::uint32_t media_sectors,
                            std::wstring& error) {
    std::uint64_t end = 0;
    const std::uint64_t count = std::min<std::uint64_t>(extent.sectors(), kMaxVolumeDescriptorSequenceSectors);
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint64_t lba = extent.location + i;
        if (lba >= media_sectors) {
            break;
        }
        const Sector sector = read_sector(read, static_cast<std::uint32_t>(lba));
        const std::optional<std::uint16_t> tag = valid_tag(sector, static_cast<std::uint32_t>(lba));
        if (!tag || *tag == kTagTerminating) {
            break;
        }
        if (*tag == kTagVolumePointer) {
            error = L"UDF volume descriptor pointers are not supported";
            return 0;
        }
        if (*tag == kTagPartition) {
            end = std::max(end, static_cast<std::uint64_t>(le32(sector, 188)) + le32(sector, 192));
        }
    }
    return end;
}

}  // namespace

std::uint16_t udf_crc(std::span<const std::uint8_t> data) noexcept {
    std::uint16_t crc = 0;
    for (const std::uint8_t byte : data) {
        crc = static_cast<std::uint16_t>(crc ^ (byte << 8));
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) != 0 ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                      : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

FsProbe probe_iso9660(const SectorReader& read, std::uint32_t media_sectors) {
    std::optional<std::uint32_t> primary;
    bool any = false;
    for (const RecognitionEntry& entry : scan_recognition(read, media_sectors)) {
        if (entry.id != "CD001") {
            break;  // ISO 9660 descriptors come first; UDF descriptors may follow (bridge discs).
        }
        any = true;
        if (entry.type == 1 && !primary) {
            primary = entry.lba;
        }
        if (entry.type == 255) {
            break;
        }
    }
    if (!any) {
        return {};
    }
    if (!primary) {
        return invalid(L"ISO 9660 has no primary volume descriptor");
    }

    const Sector pvd = read_sector(read, *primary);
    if (pvd[6] != 1) {
        return invalid(L"ISO 9660 primary volume descriptor has an unknown version");
    }
    const std::uint32_t volume_space = le32(pvd, 80);
    const std::uint16_t block_size = le16(pvd, 128);
    if (volume_space != be32(pvd, 84) || block_size != be16(pvd, 130)) {
        return invalid(L"ISO 9660 primary volume descriptor is inconsistent (both-endian fields differ)");
    }
    if (block_size != kSectorSize) {
        return invalid(L"ISO 9660 logical block size " + std::to_wstring(block_size) + L" is not supported");
    }
    if (volume_space == 0) {
        return invalid(L"ISO 9660 volume space size is zero");
    }
    return FsProbe{FsProbe::Status::Found, volume_space, {}};
}

FsProbe probe_udf(const SectorReader& read, std::uint32_t media_sectors) {
    bool beginning = false;
    bool nsr = false;
    for (const RecognitionEntry& entry : scan_recognition(read, media_sectors)) {
        if (entry.id == "BEA01") {
            beginning = true;
        } else if (beginning && (entry.id == "NSR02" || entry.id == "NSR03")) {
            nsr = true;
        } else if (entry.id == "TEA01") {
            break;
        }
    }
    if (!nsr) {
        return {};
    }

    // Anchors: LBA 256, N - 256 and N - 1. The first valid one gives the sequences.
    std::vector<std::uint32_t> candidates = {256};
    if (media_sectors > 512) {
        candidates.push_back(media_sectors - 256);
    }
    if (media_sectors > 257) {
        candidates.push_back(media_sectors - 1);
    }
    std::uint64_t end = 0;
    std::optional<Sector> anchor;
    for (const std::uint32_t lba : candidates) {
        if (lba >= media_sectors) {
            continue;
        }
        const Sector sector = read_sector(read, lba);
        if (valid_tag(sector, lba) == kTagAnchor) {
            end = std::max<std::uint64_t>(end, std::uint64_t{lba} + 1);
            if (!anchor) {
                anchor = sector;
            }
        }
    }
    if (!anchor) {
        return invalid(L"UDF has no valid anchor volume descriptor pointer");
    }

    const Extent main{le32(*anchor, 16), le32(*anchor, 20)};
    const Extent reserve{le32(*anchor, 24), le32(*anchor, 28)};
    std::wstring error;
    std::uint64_t partitions = partition_end(read, main, media_sectors, error);
    if (!error.empty()) {
        return invalid(error);
    }
    if (partitions == 0) {
        partitions = partition_end(read, reserve, media_sectors, error);
        if (!error.empty()) {
            return invalid(error);
        }
    }
    if (partitions == 0) {
        return invalid(L"UDF has no valid partition descriptor");
    }
    end = std::max({end, partitions, main.end(), reserve.end()});
    return FsProbe{FsProbe::Status::Found, end, {}};
}

}  // namespace isoforge::fs
