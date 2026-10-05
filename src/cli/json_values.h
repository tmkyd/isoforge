// SPDX-License-Identifier: Apache-2.0
//
// JSON values shared by the --json output of list, info and create.

#pragma once

#include <optional>
#include <string>

#include "imaging/range_decision.h"
#include "media/profile.h"

namespace isoforge::cli {

// A JSON string, or null.
std::wstring json_or_null(const std::optional<std::wstring>& text);

// {"code": 17, "name": "DVD-R"}, or null.
std::wstring profile_json(const std::optional<media::ProfileInfo>& profile);

// {"start": 0, "sectors": 20000, "bytes": 40960000}, or null.
std::wstring range_json(const std::optional<imaging::SectorRange>& range);

}  // namespace isoforge::cli
