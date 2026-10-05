// SPDX-License-Identifier: Apache-2.0

#include "imaging/reader.h"

#include <algorithm>
#include <limits>

#include "util/keep_awake.h"
#include "util/strings.h"

namespace isoforge::imaging {
namespace {

constexpr std::uint32_t kCdTailSectors = 2;

// Calls prevent(true) now and prevent(false) when the scope ends, whatever the outcome.
class MediaRemovalScope {
public:
    explicit MediaRemovalScope(const std::function<void(bool)>& prevent) : prevent_(prevent) {
        if (prevent_) {
            prevent_(true);
        }
    }
    ~MediaRemovalScope() {
        if (prevent_) {
            try {
                prevent_(false);
            } catch (...) {
                // Closing the device handle releases the lock as well.
            }
        }
    }
    MediaRemovalScope(const MediaRemovalScope&) = delete;
    MediaRemovalScope& operator=(const MediaRemovalScope&) = delete;

private:
    const std::function<void(bool)>& prevent_;
};

std::uint32_t chunk_sectors(device::ScsiDevice& device, const ReadOptions& options) {
    const std::size_t limit = device.transport().max_transfer_bytes() / device::kSectorSize;
    std::size_t sectors = std::max<std::size_t>(1, limit);
    if (options.sectors_per_read != 0) {
        sectors = std::min<std::size_t>(sectors, options.sectors_per_read);
    }
    return static_cast<std::uint32_t>(std::min<std::size_t>(sectors, std::numeric_limits<std::uint16_t>::max()));
}

// The state of one read_range() call.
class RangeReader {
public:
    RangeReader(device::ScsiDevice& device, const SectorRange& range, const ReadOptions& options, DataSink& sink,
                ReadObserver& observer)
        : device_(device),
          range_(range),
          options_(options),
          sink_(sink),
          observer_(observer),
          per_read_(chunk_sectors(device, options)),
          tail_start_(tail_exception_start(range, options)),
          buffer_(static_cast<std::size_t>(per_read_) * device::kSectorSize),
          sector_(device::kSectorSize) {}

    ReadResult run() {
        const auto started = std::chrono::steady_clock::now();
        for (std::uint64_t lba = range_.start; lba < range_.end();) {
            if (observer_.cancel_requested()) {
                result_.outcome = ReadOutcome::Cancelled;
                return std::move(result_);
            }
            const auto count = static_cast<std::uint16_t>(std::min<std::uint64_t>(per_read_, range_.end() - lba));
            if (!read_chunk(lba, count)) {
                return std::move(result_);
            }
            lba += count;
            observer_.on_progress(
                ReadProgress{lba - range_.start, range_.count, std::chrono::steady_clock::now() - started});
        }
        result_.outcome = ReadOutcome::Completed;
        result_.sha256 = sha_.finish();
        return std::move(result_);
    }

private:
    // Reads `count` sectors at `lba` with one command, or sector by sector when that fails.
    // Returns false when reading stops; result_ then holds the outcome.
    bool read_chunk(std::uint64_t lba, std::uint16_t count) {
        const std::span<std::uint8_t> chunk(buffer_.data(), static_cast<std::size_t>(count) * device::kSectorSize);
        try {
            device_.read10(static_cast<std::uint32_t>(lba), count, chunk);
        } catch (const device::ScsiCommandError& error) {
            observer_.on_range_failure(static_cast<std::uint32_t>(lba), count, error);
            for (std::uint64_t s = lba; s < lba + count; ++s) {
                if (!read_single(static_cast<std::uint32_t>(s))) {
                    return false;
                }
            }
            return true;
        }
        return emit(chunk);
    }

    // Reads one sector with one attempt plus `retries` more. A sector that stays unreadable fails
    // the read unless the CD tail exception lets it be excluded.
    bool read_single(std::uint32_t lba) {
        // 64-bit so that --retries 4294967295 cannot wrap around to zero attempts.
        const std::uint64_t attempts = std::uint64_t{options_.retries} + 1;
        std::optional<device::ScsiCommandError> last_error;
        for (std::uint64_t attempt = 1; attempt <= attempts; ++attempt) {
            if (attempt > 1 && options_.sleep && options_.retry_delay.count() > 0) {
                options_.sleep(options_.retry_delay);
            }
            if (observer_.cancel_requested()) {
                result_.outcome = ReadOutcome::Cancelled;
                return false;
            }
            try {
                device_.read10(lba, 1, sector_);
            } catch (const device::ScsiCommandError& error) {
                observer_.on_read_failure(lba, attempt, error);
                last_error = error;
                continue;
            }
            return emit(sector_);
        }
        if (lba < tail_start_) {
            return fail(lba, L": " + util::widen_ascii(last_error->what()));
        }
        if (result_.excluded.empty()) {
            result_.excluded_reason = util::widen_ascii(device::describe(last_error->sense()));
        }
        result_.excluded.push_back(lba);
        return true;
    }

    // Passes sectors to the sink and the hash. Excluded sectors must be the last sectors of the
    // range, so a readable sector after one fails the read.
    bool emit(std::span<const std::uint8_t> data) {
        if (!result_.excluded.empty()) {
            return fail(result_.excluded.front(),
                        L", but a later sector is readable; only trailing sectors may be excluded");
        }
        sink_.write(data);
        sha_.update(data);
        result_.sectors_written += data.size() / device::kSectorSize;
        return true;
    }

    bool fail(std::uint32_t lba, const std::wstring& detail) {
        result_.outcome = ReadOutcome::ReadError;
        result_.failed_lba = lba;
        result_.error = L"cannot read sector " + std::to_wstring(lba) + detail;
        return false;
    }

    device::ScsiDevice& device_;
    const SectorRange& range_;
    const ReadOptions& options_;
    DataSink& sink_;
    ReadObserver& observer_;
    const std::uint32_t per_read_;
    const std::uint64_t tail_start_;
    std::vector<std::uint8_t> buffer_;
    std::vector<std::uint8_t> sector_;
    util::Sha256 sha_;
    ReadResult result_;
};

}  // namespace

std::uint64_t tail_exception_start(const SectorRange& range, const ReadOptions& options) noexcept {
    if (!options.cd_tail_exception || range.count == 0) {
        return range.end();
    }
    const std::uint64_t last_two = range.end() - std::min<std::uint64_t>(kCdTailSectors, range.count);
    return std::max({last_two, options.filesystem_end, std::uint64_t{range.start}});
}

ReadResult read_range(device::ScsiDevice& device, const SectorRange& range, const ReadOptions& options,
                      DataSink& sink, ReadObserver& observer) {
    const util::KeepAwake keep_awake;
    const MediaRemovalScope removal(options.prevent_media_removal);
    return RangeReader(device, range, options, sink, observer).run();
}

}  // namespace isoforge::imaging
