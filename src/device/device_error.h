// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace isoforge::device {

// A Windows API failure while opening or controlling a device (not a SCSI command failure).
class DeviceError : public std::runtime_error {
public:
    enum class Kind {
        AccessDenied,     // ERROR_ACCESS_DENIED (exit code 6).
        NotFound,         // The device does not exist.
        NotOpticalDrive,  // The device is not FILE_DEVICE_CD_ROM.
        Other,
    };

    DeviceError(Kind kind, std::uint32_t win32_error, const std::string& message);

    // Classifies a Win32 error code from `operation` on `device_path` (ASCII device name).
    static DeviceError from_win32(std::uint32_t win32_error, const std::string& operation,
                                  const std::string& device_path);

    Kind kind() const noexcept { return kind_; }
    std::uint32_t win32_error() const noexcept { return win32_error_; }

private:
    Kind kind_;
    std::uint32_t win32_error_;
};

}  // namespace isoforge::device
