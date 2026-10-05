// SPDX-License-Identifier: Apache-2.0

#include "util/version.h"

#include "isoforge_version.h"

namespace isoforge {

const wchar_t* version() noexcept {
    return ISOFORGE_VERSION_WSTRING;
}

}  // namespace isoforge
