// SPDX-License-Identifier: Apache-2.0

#include "device/scsi_device.h"

#include <functional>
#include <limits>

#include "util/byte_order.h"
#include "util/strings.h"

namespace isoforge::device {
namespace {

using util::be32;

std::string error_message(std::uint8_t opcode, const ScsiResult& result, std::size_t expected) {
    std::string message = command_name(opcode) + " failed: ";
    if (result.status != kScsiStatusGood) {
        message += "SCSI status " + util::hex_with_suffix(result.status, 2);
        if (result.status == kScsiStatusCheckCondition) {
            message += " (CHECK CONDITION)";
        }
        message += ", " + describe(parse_sense(result.sense));
    } else {
        message += "transferred " + std::to_string(result.transferred) + " of " + std::to_string(expected) +
                   " bytes";
    }
    return message;
}

// Reads a response that starts with a big-endian length field counting the bytes after it.
std::vector<std::uint8_t> read_variable_length(ScsiDevice& device,
                                               const std::function<Cdb(std::uint16_t)>& make_cdb,
                                               std::size_t length_field_size) {
    std::vector<std::uint8_t> header(length_field_size);
    device.execute(make_cdb(static_cast<std::uint16_t>(length_field_size)), header);

    // 64-bit so that a 4-byte length field near FFFFFFFFh cannot wrap around in a 32-bit size_t.
    std::uint64_t declared = 0;
    for (const std::uint8_t b : header) {
        declared = (declared << 8) | b;
    }
    const std::uint64_t total = length_field_size + declared;
    if (total > std::numeric_limits<std::uint16_t>::max()) {
        throw ResponseError(command_name(make_cdb(0).opcode()) + " response of " + std::to_string(total) +
                            " bytes exceeds the 16-bit allocation length");
    }
    std::vector<std::uint8_t> response(static_cast<std::size_t>(total));
    device.execute(make_cdb(static_cast<std::uint16_t>(total)), response);
    return response;
}

}  // namespace

ScsiCommandError::ScsiCommandError(std::uint8_t opcode, const ScsiResult& result, std::size_t expected)
    : ScsiError(error_message(opcode, result, expected)),
      opcode_(opcode),
      status_(result.status),
      sense_(parse_sense(result.sense)),
      expected_(expected),
      transferred_(result.transferred) {}

void ScsiDevice::execute(const Cdb& cdb, std::span<std::uint8_t> data_in) {
    const ScsiResult result = transport_.execute(cdb, data_in);
    if (result.status != kScsiStatusGood || result.transferred != data_in.size()) {
        throw ScsiCommandError(cdb.opcode(), result, data_in.size());
    }
}

UnitReadiness ScsiDevice::test_unit_ready() {
    const ScsiResult result = transport_.execute(make_test_unit_ready(), {});
    if (result.status == kScsiStatusGood) {
        return UnitReadiness{true, {}};
    }
    if (result.status == kScsiStatusCheckCondition) {
        return UnitReadiness{false, parse_sense(result.sense)};
    }
    throw ScsiCommandError(opcode::kTestUnitReady, result, 0);
}

ReadCapacityData ScsiDevice::read_capacity() {
    std::vector<std::uint8_t> data(8);
    execute(make_read_capacity10(), data);
    return ReadCapacityData{be32(data, 0), be32(data, 4)};
}

void ScsiDevice::read10(std::uint32_t lba, std::uint16_t blocks, std::span<std::uint8_t> out) {
    if (out.size() != static_cast<std::size_t>(blocks) * kSectorSize) {
        throw std::invalid_argument("READ(10) buffer size does not match the block count");
    }
    if (blocks > 0 && lba > std::numeric_limits<std::uint32_t>::max() - (blocks - 1u)) {
        throw std::invalid_argument("READ(10) range exceeds the 32-bit LBA space");
    }
    execute(make_read10(lba, blocks), out);
}

std::vector<std::uint8_t> ScsiDevice::get_configuration(std::uint8_t rt, std::uint16_t starting_feature) {
    return read_variable_length(
        *this, [&](std::uint16_t length) { return make_get_configuration(rt, starting_feature, length); }, 4);
}

std::vector<std::uint8_t> ScsiDevice::read_disc_information() {
    return read_variable_length(*this, [](std::uint16_t length) { return make_read_disc_information(length); },
                                2);
}

std::vector<std::uint8_t> ScsiDevice::read_track_information(std::uint8_t address_type, std::uint32_t address) {
    return read_variable_length(
        *this, [&](std::uint16_t length) { return make_read_track_information(address_type, address, length); },
        2);
}

std::vector<std::uint8_t> ScsiDevice::read_toc(std::uint8_t format, bool msf, std::uint8_t track_or_session) {
    return read_variable_length(
        *this, [&](std::uint16_t length) { return make_read_toc(format, msf, track_or_session, length); }, 2);
}

}  // namespace isoforge::device
