// SPDX-License-Identifier: Apache-2.0

#include "media/profile.h"

#include "util/strings.h"

namespace isoforge::media {
namespace {

struct Entry {
    std::uint16_t profile;
    MediaFamily family;
    const wchar_t* name;
    bool supported;
    bool overwritable;
    const wchar_t* reason;
};

constexpr Entry kProfiles[] = {
    {0x0000, MediaFamily::None, L"no disc", false, false, L"no disc is loaded"},
    {0x0008, MediaFamily::Cd, L"CD-ROM", true, false, L""},
    {0x0009, MediaFamily::Cd, L"CD-R", true, false, L""},
    {0x000A, MediaFamily::Cd, L"CD-RW", true, false, L""},
    {0x0010, MediaFamily::Dvd, L"DVD-ROM", true, false, L""},
    {0x0011, MediaFamily::Dvd, L"DVD-R", true, false, L""},
    {0x0012, MediaFamily::Dvd, L"DVD-RAM", false, false, L"DVD-RAM is not supported"},
    {0x0013, MediaFamily::Dvd, L"DVD-RW (restricted overwrite)", true, true, L""},
    {0x0014, MediaFamily::Dvd, L"DVD-RW (sequential)", true, false, L""},
    {0x0015, MediaFamily::Dvd, L"DVD-R DL (sequential)", true, false, L""},
    {0x0016, MediaFamily::Dvd, L"DVD-R DL (layer jump)", true, false, L""},
    {0x0017, MediaFamily::Dvd, L"DVD-RW DL", true, false, L""},
    {0x0018, MediaFamily::Dvd, L"DVD-Download", false, false, L"DVD-Download discs are not supported"},
    {0x001A, MediaFamily::Dvd, L"DVD+RW", true, true, L""},
    {0x001B, MediaFamily::Dvd, L"DVD+R", true, false, L""},
    {0x002A, MediaFamily::Dvd, L"DVD+RW DL", true, true, L""},
    {0x002B, MediaFamily::Dvd, L"DVD+R DL", true, false, L""},
    {0x0040, MediaFamily::Bd, L"BD-ROM", true, false, L""},
    {0x0041, MediaFamily::Bd, L"BD-R (SRM)", true, false, L""},
    {0x0042, MediaFamily::Bd, L"BD-R (RRM)", false, false,
     L"BD-R in random recording mode (used for packet writing) is not supported"},
    {0x0043, MediaFamily::Bd, L"BD-RE", true, true, L""},
    {0x0050, MediaFamily::Other, L"HD DVD-ROM", false, false, L"HD DVD is not supported"},
    {0x0051, MediaFamily::Other, L"HD DVD-R", false, false, L"HD DVD is not supported"},
    {0x0052, MediaFamily::Other, L"HD DVD-RAM", false, false, L"HD DVD is not supported"},
    {0x0053, MediaFamily::Other, L"HD DVD-RW", false, false, L"HD DVD is not supported"},
    {0x0058, MediaFamily::Other, L"HD DVD-R DL", false, false, L"HD DVD is not supported"},
    {0x005A, MediaFamily::Other, L"HD DVD-RW DL", false, false, L"HD DVD is not supported"},
};

}  // namespace

ProfileInfo profile_info(std::uint16_t profile) {
    for (const Entry& entry : kProfiles) {
        if (entry.profile == profile) {
            return ProfileInfo{entry.profile, entry.family, entry.name, entry.supported, entry.overwritable,
                               entry.reason};
        }
    }
    const std::wstring name = L"profile " + util::widen_ascii(util::hex_with_suffix(profile, 4));
    return ProfileInfo{profile, MediaFamily::Other, name, false, false,
                       L"unknown or unsupported disc type (" + name + L")"};
}

}  // namespace isoforge::media
