// SPDX-License-Identifier: Apache-2.0

#include "util/sha256.h"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace isoforge::util {
namespace {

void check(NTSTATUS status, const char* operation) {
    if (!BCRYPT_SUCCESS(status)) {
        char code[16];
        std::snprintf(code, sizeof(code), "%08lX", static_cast<unsigned long>(status));
        throw std::runtime_error(std::string(operation) + " failed (NTSTATUS " + code + ")");
    }
}

}  // namespace

Sha256::Sha256() {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    check(::BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0),
          "BCryptOpenAlgorithmProvider");
    algorithm_ = algorithm;
    BCRYPT_HASH_HANDLE hash = nullptr;
    const NTSTATUS status = ::BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        ::BCryptCloseAlgorithmProvider(algorithm, 0);
        algorithm_ = nullptr;
        check(status, "BCryptCreateHash");
    }
    hash_ = hash;
}

Sha256::~Sha256() {
    if (hash_ != nullptr) {
        ::BCryptDestroyHash(static_cast<BCRYPT_HASH_HANDLE>(hash_));
    }
    if (algorithm_ != nullptr) {
        ::BCryptCloseAlgorithmProvider(static_cast<BCRYPT_ALG_HANDLE>(algorithm_), 0);
    }
}

void Sha256::update(std::span<const std::uint8_t> data) {
    if (finished_) {
        throw std::logic_error("Sha256::update after finish");
    }
    while (!data.empty()) {
        const std::size_t chunk = std::min<std::size_t>(data.size(), std::numeric_limits<ULONG>::max());
        check(::BCryptHashData(static_cast<BCRYPT_HASH_HANDLE>(hash_), const_cast<PUCHAR>(data.data()),
                               static_cast<ULONG>(chunk), 0),
              "BCryptHashData");
        data = data.subspan(chunk);
    }
}

Sha256Digest Sha256::finish() {
    if (finished_) {
        throw std::logic_error("Sha256::finish called twice");
    }
    Sha256Digest digest{};
    check(::BCryptFinishHash(static_cast<BCRYPT_HASH_HANDLE>(hash_), digest.data(),
                             static_cast<ULONG>(digest.size()), 0),
          "BCryptFinishHash");
    finished_ = true;
    return digest;
}

std::wstring to_hex(const Sha256Digest& digest) {
    static constexpr wchar_t kHex[] = L"0123456789abcdef";
    std::wstring text;
    text.reserve(digest.size() * 2);
    for (const std::uint8_t b : digest) {
        text += kHex[b >> 4];
        text += kHex[b & 0xF];
    }
    return text;
}

}  // namespace isoforge::util
