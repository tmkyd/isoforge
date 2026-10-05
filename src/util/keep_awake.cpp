// SPDX-License-Identifier: Apache-2.0

#include "util/keep_awake.h"

#include <windows.h>

namespace isoforge::util {

KeepAwake::KeepAwake() noexcept {
    ::SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
}

KeepAwake::~KeepAwake() {
    ::SetThreadExecutionState(ES_CONTINUOUS);
}

}  // namespace isoforge::util
