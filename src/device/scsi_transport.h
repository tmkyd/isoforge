// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "device/scsi_commands.h"

namespace isoforge::device {

inline constexpr std::uint8_t kScsiStatusGood = 0x00;
inline constexpr std::uint8_t kScsiStatusCheckCondition = 0x02;

// Raw outcome of one SCSI command as reported by the transport.
struct ScsiResult {
    std::uint8_t status = kScsiStatusGood;
    std::vector<std::uint8_t> sense;  // Raw sense data, empty when none was returned.
    std::size_t transferred = 0;      // Bytes actually transferred to the data buffer.
};

// Thrown when a command outside the allow list is requested. The command is never passed to the
// device.
class CommandNotAllowed : public std::logic_error {
public:
    explicit CommandNotAllowed(const Cdb& cdb);
};

// Executes SCSI commands on a device: SPTI for real drives, an emulation in tests.
//
// execute() is not virtual so that every implementation goes through the same checks: the
// allow list, and the transfer size limit. Only data-in and no-data commands exist because no
// allowed command sends data to the device.
class ScsiTransport {
public:
    virtual ~ScsiTransport() = default;

    // Throws CommandNotAllowed, std::invalid_argument (buffer larger than max_transfer_bytes),
    // or an implementation-specific error such as DeviceError when the command cannot be issued.
    ScsiResult execute(const Cdb& cdb, std::span<std::uint8_t> data_in);

    // Largest data buffer that one command may use.
    virtual std::size_t max_transfer_bytes() const = 0;

private:
    virtual ScsiResult do_execute(const Cdb& cdb, std::span<std::uint8_t> data_in) = 0;
};

}  // namespace isoforge::device
