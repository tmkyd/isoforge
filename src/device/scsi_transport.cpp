// SPDX-License-Identifier: Apache-2.0

#include "device/scsi_transport.h"

#include <string>

namespace isoforge::device {

CommandNotAllowed::CommandNotAllowed(const Cdb& cdb)
    : std::logic_error("SCSI command " + command_name(cdb.opcode()) + " with a " +
                       std::to_string(cdb.length) + "-byte CDB is not in the allow list") {}

ScsiResult ScsiTransport::execute(const Cdb& cdb, std::span<std::uint8_t> data_in) {
    if (!is_allowed(cdb)) {
        throw CommandNotAllowed(cdb);
    }
    if (data_in.size() > max_transfer_bytes()) {
        throw std::invalid_argument("data buffer of " + std::to_string(data_in.size()) +
                                    " bytes exceeds the transfer limit of " +
                                    std::to_string(max_transfer_bytes()) + " bytes");
    }
    return do_execute(cdb, data_in);
}

}  // namespace isoforge::device
