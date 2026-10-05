// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <functional>

#include "cli/drive_provider.h"
#include "util/file_system.h"

namespace isoforge::cli {

// Everything a command needs from the outside world; tests substitute fakes.
struct Services {
    DriveProvider& drives;
    util::FileSystem& files;
    std::function<bool()> interrupted;  // True after Ctrl+C.
};

// Thrown when a command notices Ctrl+C while waiting; the command ends with exit code 130.
struct Interrupted {};

// Installs a console control handler for its lifetime: Ctrl+C, Ctrl+Break and closing the
// console set a flag instead of ending the process, so that commands can clean up and end with
// exit code 130.
class InterruptHandler {
public:
    InterruptHandler();
    ~InterruptHandler();
    InterruptHandler(const InterruptHandler&) = delete;
    InterruptHandler& operator=(const InterruptHandler&) = delete;

    static bool requested() noexcept;
};

}  // namespace isoforge::cli
