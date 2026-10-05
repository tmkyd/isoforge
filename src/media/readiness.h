// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "device/scsi_device.h"

namespace isoforge::media {

enum class MediaState { Loaded, NoDisc, NotReady, Unknown };

struct MediaStatus {
    MediaState state = MediaState::Unknown;
    device::SenseData sense;  // Valid unless the state is Loaded.
};

// One TEST UNIT READY, without waiting (used by list). A UNIT ATTENTION, which drives report once
// after a disc change, is followed by one more TEST UNIT READY.
MediaStatus probe_media(device::ScsiDevice& device);

inline constexpr std::chrono::milliseconds kReadyTimeout{30'000};
inline constexpr std::chrono::milliseconds kReadyPollInterval{1'000};

using Sleeper = std::function<void(std::chrono::milliseconds)>;

// Repeats TEST UNIT READY until the drive is ready or `timeout` has been spent waiting.
// Returns the last status.
MediaStatus wait_until_ready(device::ScsiDevice& device, const Sleeper& sleep,
                             std::chrono::milliseconds timeout = kReadyTimeout,
                             std::chrono::milliseconds interval = kReadyPollInterval);

std::wstring describe(MediaState state);

}  // namespace isoforge::media
