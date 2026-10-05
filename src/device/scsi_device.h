// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "device/scsi_commands.h"
#include "device/scsi_sense.h"
#include "device/scsi_transport.h"

namespace isoforge::device {

// The drive gave no usable answer to a command: the command failed (ScsiCommandError) or its
// response cannot be used (ResponseError). Steps that only record what could not be read catch
// this; Windows errors (DeviceError) end the command instead.
class ScsiError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A SCSI command that did not succeed: status other than GOOD, or fewer bytes than requested.
class ScsiCommandError : public ScsiError {
public:
    ScsiCommandError(std::uint8_t opcode, const ScsiResult& result, std::size_t expected);

    std::uint8_t opcode() const noexcept { return opcode_; }
    std::uint8_t status() const noexcept { return status_; }
    const SenseData& sense() const noexcept { return sense_; }
    std::size_t expected() const noexcept { return expected_; }
    std::size_t transferred() const noexcept { return transferred_; }

private:
    std::uint8_t opcode_;
    std::uint8_t status_;
    SenseData sense_;
    std::size_t expected_;
    std::size_t transferred_;
};

// A response that is too short, inconsistent, or longer than an allocation length can request.
class ResponseError : public ScsiError {
public:
    using ScsiError::ScsiError;
};

struct ReadCapacityData {
    std::uint32_t last_lba = 0;
    std::uint32_t block_length = 0;
};

struct UnitReadiness {
    bool ready = false;
    SenseData sense;  // Valid when not ready.
};

// Issues the allowed commands through a transport and judges their success: a command succeeds
// only when the status is GOOD and the transfer has the requested length.
class ScsiDevice {
public:
    explicit ScsiDevice(ScsiTransport& transport) noexcept : transport_(transport) {}

    ScsiTransport& transport() noexcept { return transport_; }

    // Throws ScsiCommandError unless the status is GOOD and data_in was filled completely.
    void execute(const Cdb& cdb, std::span<std::uint8_t> data_in);

    // CHECK CONDITION is reported as not ready with its sense data; other failures throw.
    UnitReadiness test_unit_ready();

    ReadCapacityData read_capacity();

    // Reads `blocks` sectors of kSectorSize bytes into `out`, which must hold exactly that many.
    void read10(std::uint32_t lba, std::uint16_t blocks, std::span<std::uint8_t> out);

    // Commands with variable-length responses. The response header is read first to learn the
    // length, then the whole response is read with exactly that allocation length.
    std::vector<std::uint8_t> get_configuration(std::uint8_t rt, std::uint16_t starting_feature);
    std::vector<std::uint8_t> read_disc_information();
    std::vector<std::uint8_t> read_track_information(std::uint8_t address_type, std::uint32_t address);
    std::vector<std::uint8_t> read_toc(std::uint8_t format, bool msf, std::uint8_t track_or_session);

private:
    ScsiTransport& transport_;
};

}  // namespace isoforge::device
