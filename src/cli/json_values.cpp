// SPDX-License-Identifier: Apache-2.0

#include "cli/json_values.h"

#include "util/json.h"

namespace isoforge::cli {

std::wstring json_or_null(const std::optional<std::wstring>& text) {
    return text ? util::json_string(*text) : L"null";
}

std::wstring profile_json(const std::optional<media::ProfileInfo>& profile) {
    if (!profile) {
        return L"null";
    }
    return L"{\"code\": " + std::to_wstring(profile->profile) + L", \"name\": " + util::json_string(profile->name) +
           L"}";
}

std::wstring range_json(const std::optional<imaging::SectorRange>& range) {
    if (!range) {
        return L"null";
    }
    return L"{\"start\": " + std::to_wstring(range->start) + L", \"sectors\": " + std::to_wstring(range->count) +
           L", \"bytes\": " + std::to_wstring(range->bytes()) + L"}";
}

}  // namespace isoforge::cli
