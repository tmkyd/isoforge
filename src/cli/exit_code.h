// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace isoforge::cli {

enum class ExitCode : int {
    Success = 0,
    UsageError = 1,
    Unsupported = 2,  // Unsupported disc or layout, undetermined range, no disc, or not an optical drive.
    ReadError = 3,
    OutputError = 4,  // Output write or I/O error, output exists, not enough free space, file size limit.
    HashMismatch = 5,
    PermissionDenied = 6,
    OtherError = 7,   // Unclassified Windows API error or internal error.
    Interrupted = 130,
};

constexpr int to_int(ExitCode code) noexcept {
    return static_cast<int>(code);
}

}  // namespace isoforge::cli
