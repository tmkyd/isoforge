// SPDX-License-Identifier: Apache-2.0
//
// User-facing output of the create and verify commands: verbosity levels, the progress line and
// the log file.

#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cli/command_line.h"
#include "cli/console.h"
#include "imaging/range_decision.h"
#include "util/file_system.h"

namespace isoforge::cli {

// "4.03 GB" (decimal units).
std::wstring format_bytes(std::uint64_t bytes);
// "LBA 0-1968879 (1968880 sectors, 4032266240 bytes)".
std::wstring format_range(const imaging::SectorRange& range);
// "PIONEER BD-RW BDR-209MIO 1.54", skipping empty parts.
std::wstring format_model(std::wstring_view vendor, std::wstring_view product, std::wstring_view revision);
// "7m25s", "1h02m03s", "12s".
std::wstring format_duration(std::chrono::steady_clock::duration duration);

// Width of the labels of the create and verify result lines, including the colon and spaces.
constexpr std::size_t kResultLabelWidth = 12;

// `label` padded to `width` (with at least one space) followed by `value`, e.g.
// labeled(L"SHA-256:", hash) is "SHA-256:    <hash>".
std::wstring labeled(std::wstring_view label, std::wstring_view value, std::size_t width = kResultLabelWidth);

// Appends timestamped lines to the --log file. A write failure stops logging with one warning;
// it never fails the command.
class Log {
public:
    Log() = default;
    Log(std::unique_ptr<util::WritableFile> file, Console& console);
    ~Log();
    Log(Log&&) noexcept = default;
    Log& operator=(Log&&) noexcept = default;

    void line(std::wstring_view text);

private:
    std::unique_ptr<util::WritableFile> file_;
    Console* console_ = nullptr;
};

// Routes messages by verbosity. Results (the SHA-256) and problems are always shown.
// With text_output false (create --json), nothing is written to standard output; the caller
// writes JSON there instead. Warnings and errors still go to standard error.
class Reporter {
public:
    Reporter(Console& console, Verbosity verbosity, Log& log, bool text_output = true)
        : console_(console), verbosity_(verbosity), log_(log), text_output_(text_output) {}

    void info(std::wstring_view text);     // Normal and verbose; logged.
    void detail(std::wstring_view text);   // Verbose only; logged.
    void result(std::wstring_view text);   // Always on standard output; logged.
    void warning(std::wstring_view text);  // Always on standard error; logged.
    void error(std::wstring_view text);    // Always on standard error; logged; kept in errors().
    // A further line of the last error, such as one reason of a list; without the prefix.
    void error_detail(std::wstring_view text);
    void log_only(std::wstring_view text) { log_.line(text); }

    const std::vector<std::wstring>& errors() const noexcept { return errors_; }

private:
    Console& console_;
    Verbosity verbosity_;
    Log& log_;
    bool text_output_;
    std::vector<std::wstring> errors_;
};

// A single progress line on an interactive console, redrawn at most twice per second, e.g.
// "Reading:      2.3%  93.8 MB / 4.03 GB  6.0 MB/s  ETA 10m55s".
class ProgressLine {
public:
    // `label` ends with a colon, e.g. L"Reading:". The value starts in the same column as in the
    // other result lines, such as "SHA-256:    <hash>".
    ProgressLine(Console& console, bool enabled, std::wstring label);
    ~ProgressLine();

    void update(std::uint64_t sectors_done, std::uint64_t sectors_total, std::chrono::steady_clock::duration elapsed);
    void update_bytes(std::uint64_t bytes_done, std::uint64_t bytes_total, std::chrono::steady_clock::duration elapsed);
    // Ends the line so that later output starts on a new line.
    void finish();

private:
    Console& console_;
    bool enabled_;
    std::wstring label_;
    bool drawn_ = false;
    std::chrono::steady_clock::time_point last_draw_{};
};

}  // namespace isoforge::cli
