// SPDX-License-Identifier: Apache-2.0

#include "device/scsi_commands.h"

#include "util/byte_order.h"
#include "util/strings.h"

namespace isoforge::device {
namespace {

using util::put_be16;
using util::put_be32;

struct AllowedCommand {
    std::uint8_t opcode;
    std::uint8_t cdb_length;
    const char* name;
};

constexpr AllowedCommand kAllowedCommands[] = {
    {opcode::kTestUnitReady, 6, "TEST UNIT READY"},
    {opcode::kReadCapacity10, 10, "READ CAPACITY(10)"},
    {opcode::kRead10, 10, "READ(10)"},
    {opcode::kReadTocPmaAtip, 10, "READ TOC/PMA/ATIP"},
    {opcode::kGetConfiguration, 10, "GET CONFIGURATION"},
    {opcode::kReadDiscInformation, 10, "READ DISC INFORMATION"},
    {opcode::kReadTrackInformation, 10, "READ TRACK INFORMATION"},
};

Cdb make(std::uint8_t op, std::uint8_t length) {
    Cdb cdb;
    cdb.bytes[0] = op;
    cdb.length = length;
    return cdb;
}

}  // namespace

std::string command_name(std::uint8_t op) {
    for (const AllowedCommand& command : kAllowedCommands) {
        if (command.opcode == op) {
            return command.name;
        }
    }
    return "opcode " + util::hex_with_suffix(op, 2);
}

bool is_allowed(const Cdb& cdb) noexcept {
    for (const AllowedCommand& command : kAllowedCommands) {
        if (command.opcode == cdb.opcode()) {
            return command.cdb_length == cdb.length;
        }
    }
    return false;
}

Cdb make_test_unit_ready() {
    return make(opcode::kTestUnitReady, 6);
}

Cdb make_read_capacity10() {
    return make(opcode::kReadCapacity10, 10);
}

Cdb make_read10(std::uint32_t lba, std::uint16_t blocks) {
    Cdb cdb = make(opcode::kRead10, 10);
    put_be32(cdb.bytes, 2, lba);
    put_be16(cdb.bytes, 7, blocks);
    return cdb;
}

Cdb make_read_toc(std::uint8_t format, bool msf, std::uint8_t track_or_session,
                  std::uint16_t allocation_length) {
    Cdb cdb = make(opcode::kReadTocPmaAtip, 10);
    cdb.bytes[1] = msf ? 0x02 : 0x00;
    cdb.bytes[2] = format & 0x0F;
    cdb.bytes[6] = track_or_session;
    put_be16(cdb.bytes, 7, allocation_length);
    return cdb;
}

Cdb make_get_configuration(std::uint8_t rt, std::uint16_t starting_feature,
                           std::uint16_t allocation_length) {
    Cdb cdb = make(opcode::kGetConfiguration, 10);
    cdb.bytes[1] = rt & 0x03;
    put_be16(cdb.bytes, 2, starting_feature);
    put_be16(cdb.bytes, 7, allocation_length);
    return cdb;
}

Cdb make_read_disc_information(std::uint16_t allocation_length) {
    Cdb cdb = make(opcode::kReadDiscInformation, 10);
    put_be16(cdb.bytes, 7, allocation_length);
    return cdb;
}

Cdb make_read_track_information(std::uint8_t address_type, std::uint32_t address,
                                std::uint16_t allocation_length) {
    Cdb cdb = make(opcode::kReadTrackInformation, 10);
    cdb.bytes[1] = address_type & 0x03;
    put_be32(cdb.bytes, 2, address);
    put_be16(cdb.bytes, 7, allocation_length);
    return cdb;
}

}  // namespace isoforge::device
