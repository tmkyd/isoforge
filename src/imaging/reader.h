// SPDX-License-Identifier: Apache-2.0
//
// The reading engine: reads the adopted range with READ(10), passes the data to a sink and
// computes its SHA-256. File output belongs to the create command.

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "device/scsi_device.h"
#include "imaging/range_decision.h"
#include "util/sha256.h"

namespace isoforge::imaging {

// Receives the sectors in order. Exceptions thrown by write() propagate out of read_range().
class DataSink {
public:
    virtual ~DataSink() = default;
    virtual void write(std::span<const std::uint8_t> data) = 0;
};

struct ReadProgress {
    std::uint64_t sectors_done = 0;
    std::uint64_t sectors_total = 0;
    std::chrono::steady_clock::duration elapsed{};
};

// Optional callbacks during reading.
class ReadObserver {
public:
    virtual ~ReadObserver() = default;
    virtual void on_progress(const ReadProgress&) {}
    // A multi-sector read failed and is about to be split into single sectors.
    virtual void on_range_failure(std::uint32_t /*lba*/, std::uint32_t /*sectors*/,
                                  const device::ScsiCommandError& /*error*/) {}
    // A single-sector read failed; `attempt` counts from 1 (the first single-sector read).
    virtual void on_read_failure(std::uint32_t /*lba*/, std::uint64_t /*attempt*/,
                                 const device::ScsiCommandError& /*error*/) {}
    // Checked before each read; true stops reading with ReadOutcome::Cancelled.
    virtual bool cancel_requested() { return false; }
};

// Pause before each retry of a single-sector read.
inline constexpr std::chrono::milliseconds kRetryDelay{500};

struct ReadOptions {
    std::uint32_t retries = 3;  // --retries: retries of each single-sector read.
    // Waits `retry_delay` before each retry when set (create passes DriveProvider::sleep).
    std::function<void(std::chrono::milliseconds)> sleep;
    std::chrono::milliseconds retry_delay = kRetryDelay;
    // Sectors per READ(10); 0 = as many as the transport's transfer limit allows.
    std::uint32_t sectors_per_read = 0;
    // The CD tail exception applies (CD media only).
    bool cd_tail_exception = false;
    // End of the file system range; excluded sectors must lie at or after it.
    std::uint64_t filesystem_end = 0;
    // Called with true before reading and false afterwards, also on failure.
    std::function<void(bool)> prevent_media_removal;
};

enum class ReadOutcome { Completed, ReadError, Cancelled };

struct ReadResult {
    ReadOutcome outcome = ReadOutcome::ReadError;
    std::uint64_t sectors_written = 0;
    std::optional<util::Sha256Digest> sha256;  // Set when Completed.
    std::vector<std::uint32_t> excluded;       // LBAs excluded by the CD tail exception.
    std::wstring excluded_reason;              // Sense data of the excluded sectors.
    std::optional<std::uint32_t> failed_lba;   // Set on ReadError.
    std::wstring error;                        // Set on ReadError.
};

// Reads `range` in order. A failed multi-sector read is split into single sectors and each gets
// `retries` more attempts. An unreadable sector fails the read unless the CD tail exception
// applies: it lies in the last two sectors of the track, at or after the file system end, and
// every sector after it is unreadable as well. Windows API errors (device::DeviceError) and sink
// errors propagate as exceptions.
ReadResult read_range(device::ScsiDevice& device, const SectorRange& range, const ReadOptions& options,
                      DataSink& sink, ReadObserver& observer);

// The first LBA that the CD tail exception may exclude: the last two sectors of the range, but
// not before the file system end. Equals range.end() when the exception does not apply.
std::uint64_t tail_exception_start(const SectorRange& range, const ReadOptions& options) noexcept;

}  // namespace isoforge::imaging
