// SPDX-License-Identifier: Apache-2.0
//
// Detection of the ISO 9660 and UDF volume ranges. The parsers read sectors through a callback so
// that real drives (READ(10) via SPTI) and tests share the same code.

#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>

namespace isoforge::fs {

// Reads one 2048-byte sector. Throws (e.g. device::ScsiCommandError) when the sector cannot be read.
using SectorReader = std::function<void(std::uint32_t lba, std::span<std::uint8_t> out)>;

struct FsProbe {
    enum class Status {
        Absent,   // No such file system.
        Found,    // end_sector is valid.
        Invalid,  // The file system is present but cannot be used to determine the range.
    };
    Status status = Status::Absent;
    std::uint64_t end_sector = 0;  // One past the last sector used by the file system.
    std::wstring detail;           // Why the file system is invalid.
};

// ISO 9660: the Primary Volume Descriptor's Volume Space Size with a 2048-byte logical block.
FsProbe probe_iso9660(const SectorReader& read, std::uint32_t media_sectors);

// UDF: the end of the partitions, the volume descriptor sequences and the anchors
// (LBA 256, N - 256, N - 1 with N = media_sectors).
FsProbe probe_udf(const SectorReader& read, std::uint32_t media_sectors);

// CRC of ECMA-167 descriptor tags (CRC-16/CCITT, polynomial 1021h, initial value 0).
std::uint16_t udf_crc(std::span<const std::uint8_t> data) noexcept;

}  // namespace isoforge::fs
