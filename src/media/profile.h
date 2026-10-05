// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <string>

namespace isoforge::media {

enum class MediaFamily { None, Cd, Dvd, Bd, Other };

// An MMC profile (GET CONFIGURATION Current Profile) and how isoforge treats it.
struct ProfileInfo {
    std::uint16_t profile = 0;
    MediaFamily family = MediaFamily::Other;
    std::wstring name;        // e.g. L"BD-R (SRM)"; L"profile 0099h" when unknown.
    bool supported = false;
    bool overwritable = false;  // DVD-RW restricted overwrite, DVD+RW, DVD+RW DL, BD-RE.
    std::wstring reason;        // Why the profile is not supported; empty when supported.
};

ProfileInfo profile_info(std::uint16_t profile);

}  // namespace isoforge::media
