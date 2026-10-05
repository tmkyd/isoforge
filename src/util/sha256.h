// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace isoforge::util {

using Sha256Digest = std::array<std::uint8_t, 32>;

// Incremental SHA-256 with the Windows CNG BCrypt API.
class Sha256 {
public:
    Sha256();  // Throws std::runtime_error when CNG fails.
    ~Sha256();
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;

    void update(std::span<const std::uint8_t> data);
    // Returns the digest; the object cannot be updated afterwards.
    Sha256Digest finish();

private:
    void* algorithm_ = nullptr;  // BCRYPT_ALG_HANDLE
    void* hash_ = nullptr;       // BCRYPT_HASH_HANDLE
    bool finished_ = false;
};

// Lower-case hexadecimal, as written by sha256sum.
std::wstring to_hex(const Sha256Digest& digest);

}  // namespace isoforge::util
