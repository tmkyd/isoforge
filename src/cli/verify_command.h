// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "cli/command_line.h"
#include "cli/console.h"
#include "cli/services.h"
#include "util/sha256.h"

namespace isoforge::cli {

// A 64-digit hexadecimal SHA-256 (either case), or std::nullopt.
std::optional<util::Sha256Digest> parse_digest(std::wstring_view text);

// The first line of a sha256sum-format hash file.
struct HashFileEntry {
    util::Sha256Digest digest{};
    std::wstring file_name;  // The file name after the digest; empty when the line has none.
};

// The first non-empty line of a sha256sum-format hash file (UTF-8, optional BOM):
// "<64 hex digits>", "<64 hex digits> *<name>" or "<64 hex digits>  <name>".
std::optional<HashFileEntry> parse_hash_file(std::string_view content);

// isoforge verify: 0 when the SHA-256 matches, 5 when it does not, 4 when a
// file cannot be read or the hash file is malformed. A hash file naming another file only warns.
int run_verify(const VerifyOptions& options, Console& console, const Services& services);

}  // namespace isoforge::cli
