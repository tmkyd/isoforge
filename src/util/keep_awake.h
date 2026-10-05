// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace isoforge::util {

// Keeps the system from sleeping while it exists: SetThreadExecutionState with
// ES_SYSTEM_REQUIRED, restored to ES_CONTINUOUS alone on destruction.
class KeepAwake {
public:
    KeepAwake() noexcept;
    ~KeepAwake();
    KeepAwake(const KeepAwake&) = delete;
    KeepAwake& operator=(const KeepAwake&) = delete;
};

}  // namespace isoforge::util
