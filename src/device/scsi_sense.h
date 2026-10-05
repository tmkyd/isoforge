// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace isoforge::device {

// Sense key, ASC and ASCQ from fixed-format (70h/71h) or descriptor-format (72h/73h) sense data.
struct SenseData {
    bool valid = false;
    std::uint8_t sense_key = 0;
    std::uint8_t asc = 0;
    std::uint8_t ascq = 0;

    bool operator==(const SenseData&) const = default;
};

namespace sense_key {
inline constexpr std::uint8_t kNotReady = 0x2;
inline constexpr std::uint8_t kMediumError = 0x3;
inline constexpr std::uint8_t kHardwareError = 0x4;
inline constexpr std::uint8_t kIllegalRequest = 0x5;
inline constexpr std::uint8_t kUnitAttention = 0x6;
}  // namespace sense_key

SenseData parse_sense(std::span<const std::uint8_t> raw) noexcept;

// e.g. "sense key 3h (MEDIUM ERROR), ASC 11h, ASCQ 00h".
std::string describe(const SenseData& sense);

}  // namespace isoforge::device
