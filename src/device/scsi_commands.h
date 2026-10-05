// SPDX-License-Identifier: Apache-2.0
//
// CDBs of the SCSI/MMC commands that isoforge may issue.

#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace isoforge::device {

namespace opcode {
inline constexpr std::uint8_t kTestUnitReady = 0x00;
inline constexpr std::uint8_t kReadCapacity10 = 0x25;
inline constexpr std::uint8_t kRead10 = 0x28;
inline constexpr std::uint8_t kReadTocPmaAtip = 0x43;
inline constexpr std::uint8_t kGetConfiguration = 0x46;
inline constexpr std::uint8_t kReadDiscInformation = 0x51;
inline constexpr std::uint8_t kReadTrackInformation = 0x52;
}  // namespace opcode

inline constexpr std::uint32_t kSectorSize = 2048;

struct Cdb {
    std::array<std::uint8_t, 16> bytes{};
    std::uint8_t length = 0;

    std::uint8_t opcode() const noexcept { return bytes[0]; }
};

// Name of an opcode for messages, e.g. "READ(10)"; "opcode 2Ah" for unknown opcodes.
std::string command_name(std::uint8_t opcode);

// The allow list: true only for the seven commands above with their standard CDB length.
bool is_allowed(const Cdb& cdb) noexcept;

Cdb make_test_unit_ready();
Cdb make_read_capacity10();
Cdb make_read10(std::uint32_t lba, std::uint16_t blocks);
// format: 0 = formatted TOC, 1 = session information, 2 = full TOC.
Cdb make_read_toc(std::uint8_t format, bool msf, std::uint8_t track_or_session,
                  std::uint16_t allocation_length);
// rt: 0 = all features, 1 = current features, 2 = one feature.
Cdb make_get_configuration(std::uint8_t rt, std::uint16_t starting_feature,
                           std::uint16_t allocation_length);
Cdb make_read_disc_information(std::uint16_t allocation_length);
// address_type: 0 = LBA, 1 = track number, 2 = session number.
Cdb make_read_track_information(std::uint8_t address_type, std::uint32_t address,
                                std::uint16_t allocation_length);

}  // namespace isoforge::device
