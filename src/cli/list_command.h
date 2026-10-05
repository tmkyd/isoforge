// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "cli/command_line.h"
#include "cli/console.h"
#include "cli/services.h"

namespace isoforge::cli {

// isoforge list. Ctrl+C between drives ends the command with exit code 130.
int run_list(const ListOptions& options, Console& console, const Services& services);

}  // namespace isoforge::cli
