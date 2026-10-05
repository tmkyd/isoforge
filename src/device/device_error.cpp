// SPDX-License-Identifier: Apache-2.0

#include "device/device_error.h"

#include <windows.h>

namespace isoforge::device {

DeviceError::DeviceError(Kind kind, std::uint32_t win32_error, const std::string& message)
    : std::runtime_error(message), kind_(kind), win32_error_(win32_error) {}

DeviceError DeviceError::from_win32(std::uint32_t win32_error, const std::string& operation,
                                    const std::string& device_path) {
    Kind kind = Kind::Other;
    std::string reason;
    switch (win32_error) {
    case ERROR_ACCESS_DENIED:
        kind = Kind::AccessDenied;
        reason = "access denied";
        break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        kind = Kind::NotFound;
        reason = "device not found";
        break;
    default:
        reason = "failed";
        break;
    }
    return DeviceError(kind, win32_error,
                       operation + " " + device_path + ": " + reason + " (Windows error " +
                           std::to_string(win32_error) + ")");
}

}  // namespace isoforge::device
