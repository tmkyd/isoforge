// SPDX-License-Identifier: Apache-2.0
//
// Reading and writing big-endian (SCSI/MMC, ISO 9660) and little-endian (ECMA-167, ISO 9660) integers.

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace isoforge::util {

inline std::uint16_t be16(std::span<const std::uint8_t> b, std::size_t offset) {
    return static_cast<std::uint16_t>((b[offset] << 8) | b[offset + 1]);
}

inline std::uint32_t be32(std::span<const std::uint8_t> b, std::size_t offset) {
    return (static_cast<std::uint32_t>(b[offset]) << 24) | (static_cast<std::uint32_t>(b[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(b[offset + 2]) << 8) | b[offset + 3];
}

inline std::uint16_t le16(std::span<const std::uint8_t> b, std::size_t offset) {
    return static_cast<std::uint16_t>(b[offset] | (b[offset + 1] << 8));
}

inline std::uint32_t le32(std::span<const std::uint8_t> b, std::size_t offset) {
    return static_cast<std::uint32_t>(b[offset]) | (static_cast<std::uint32_t>(b[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(b[offset + 2]) << 16) | (static_cast<std::uint32_t>(b[offset + 3]) << 24);
}

inline void put_be16(std::span<std::uint8_t> b, std::size_t offset, std::uint16_t value) {
    b[offset] = static_cast<std::uint8_t>(value >> 8);
    b[offset + 1] = static_cast<std::uint8_t>(value);
}

inline void put_be32(std::span<std::uint8_t> b, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        b[offset + i] = static_cast<std::uint8_t>(value >> (8 * (3 - i)));
    }
}

inline void put_le16(std::span<std::uint8_t> b, std::size_t offset, std::uint16_t value) {
    b[offset] = static_cast<std::uint8_t>(value);
    b[offset + 1] = static_cast<std::uint8_t>(value >> 8);
}

inline void put_le32(std::span<std::uint8_t> b, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        b[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
    }
}

}  // namespace isoforge::util
