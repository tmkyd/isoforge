// SPDX-License-Identifier: Apache-2.0

#include "media/readiness.h"

namespace isoforge::media {
namespace {

constexpr std::uint8_t kAscMediumNotPresent = 0x3A;

MediaStatus classify(const device::UnitReadiness& readiness) {
    if (readiness.ready) {
        return MediaStatus{MediaState::Loaded, {}};
    }
    const device::SenseData& sense = readiness.sense;
    if (sense.valid && sense.sense_key == device::sense_key::kNotReady) {
        return MediaStatus{sense.asc == kAscMediumNotPresent ? MediaState::NoDisc : MediaState::NotReady, sense};
    }
    return MediaStatus{MediaState::Unknown, sense};
}

bool is_unit_attention(const device::UnitReadiness& readiness) {
    return !readiness.ready && readiness.sense.valid &&
           readiness.sense.sense_key == device::sense_key::kUnitAttention;
}

}  // namespace

// TEST UNIT READY; a status other than GOOD or CHECK CONDITION (e.g. BUSY while another program
// uses the drive) means the drive is not ready, not that isoforge failed.
MediaStatus probe_media(device::ScsiDevice& device) {
    try {
        device::UnitReadiness readiness = device.test_unit_ready();
        if (is_unit_attention(readiness)) {
            readiness = device.test_unit_ready();
        }
        return classify(readiness);
    } catch (const device::ScsiCommandError& error) {
        return MediaStatus{MediaState::Unknown, error.sense()};
    }
}

MediaStatus wait_until_ready(device::ScsiDevice& device, const Sleeper& sleep, std::chrono::milliseconds timeout,
                             std::chrono::milliseconds interval) {
    std::chrono::milliseconds waited{0};
    for (;;) {
        const MediaStatus status = probe_media(device);
        if (status.state == MediaState::Loaded || waited >= timeout) {
            return status;
        }
        sleep(interval);
        waited += interval;
    }
}

std::wstring describe(MediaState state) {
    switch (state) {
    case MediaState::Loaded:
        return L"loaded";
    case MediaState::NoDisc:
        return L"no disc";
    case MediaState::NotReady:
        return L"not ready";
    case MediaState::Unknown:
        break;
    }
    return L"unknown";
}

}  // namespace isoforge::media
