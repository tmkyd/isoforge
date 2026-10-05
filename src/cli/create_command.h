// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <string_view>

#include "cli/command_line.h"
#include "cli/console.h"
#include "cli/services.h"
#include "util/sha256.h"

namespace isoforge::cli {

// The hash file line in sha256sum format: "<64 lower-case hex> *<file name>\n".
std::wstring hash_file_line(const util::Sha256Digest& digest, const std::wstring& iso_path);

// Before reading the disc: output and hash files must not exist (unless --overwrite), the output
// directory must exist, the disc must be supported with a determined range, and the output volume
// must have the space and file size limit for the ISO. The ISO is written to <output>.partial and
// renamed only after everything succeeded (including --verify); the hash file is written last.
// `command_line` is written to the --log file. With --json, the result is written as JSON to
// standard output whatever the outcome.
int run_create(const CreateOptions& options, Console& console, const Services& services,
               std::wstring_view command_line);

}  // namespace isoforge::cli
