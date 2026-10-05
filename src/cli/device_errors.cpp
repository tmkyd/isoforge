// SPDX-License-Identifier: Apache-2.0

#include "cli/device_errors.h"

#include "util/strings.h"

namespace isoforge::cli {

ExitCode exit_code_for(const device::DeviceError& error) noexcept {
    switch (error.kind()) {
    case device::DeviceError::Kind::AccessDenied:
        return ExitCode::PermissionDenied;
    case device::DeviceError::Kind::NotFound:
    case device::DeviceError::Kind::NotOpticalDrive:
        return ExitCode::Unsupported;
    case device::DeviceError::Kind::Other:
        break;
    }
    return ExitCode::OtherError;
}

std::wstring describe(const device::DeviceError& error) {
    std::wstring message = util::widen_ascii(error.what());
    if (error.kind() == device::DeviceError::Kind::AccessDenied) {
        message += L"\nRun isoforge as administrator to access the drive.";
    }
    return message;
}

}  // namespace isoforge::cli
