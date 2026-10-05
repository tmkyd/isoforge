// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>
#include <string>

#include "cli/exit_code.h"
#include "device/device_error.h"

namespace isoforge::cli {

// Exit code for a device error: insufficient permission -> 6, a missing drive or a device that is
// not an optical drive -> 2, anything else -> 7.
ExitCode exit_code_for(const device::DeviceError& error) noexcept;

// User-facing message for a device error, with the administrator hint for AccessDenied.
std::wstring describe(const device::DeviceError& error);

}  // namespace isoforge::cli
