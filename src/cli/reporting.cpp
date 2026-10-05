// SPDX-License-Identifier: Apache-2.0

#include "cli/reporting.h"

#include <windows.h>

#include <cstdio>

#include "cli/style.h"
#include "device/scsi_commands.h"
#include "util/utf8.h"

namespace isoforge::cli {

std::wstring format_bytes(std::uint64_t bytes) {
    wchar_t buffer[32];
    if (bytes >= 1'000'000'000ull) {
        std::swprintf(buffer, 32, L"%.2f GB", static_cast<double>(bytes) / 1e9);
    } else if (bytes >= 1'000'000ull) {
        std::swprintf(buffer, 32, L"%.1f MB", static_cast<double>(bytes) / 1e6);
    } else if (bytes >= 1'000ull) {
        std::swprintf(buffer, 32, L"%.1f kB", static_cast<double>(bytes) / 1e3);
    } else {
        std::swprintf(buffer, 32, L"%llu bytes", static_cast<unsigned long long>(bytes));
    }
    return buffer;
}

std::wstring format_range(const imaging::SectorRange& range) {
    return L"LBA " + std::to_wstring(range.start) + L"-" + std::to_wstring(range.end() - 1) + L" (" +
           std::to_wstring(range.count) + L" sectors, " + std::to_wstring(range.bytes()) + L" bytes)";
}

std::wstring format_model(std::wstring_view vendor, std::wstring_view product, std::wstring_view revision) {
    std::wstring model;
    for (const std::wstring_view part : {vendor, product, revision}) {
        if (!part.empty()) {
            model += (model.empty() ? L"" : L" ") + std::wstring(part);
        }
    }
    return model;
}

std::wstring format_duration(std::chrono::steady_clock::duration duration) {
    const long long total = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    const long long hours = total / 3600;
    const long long minutes = (total % 3600) / 60;
    const long long seconds = total % 60;
    wchar_t buffer[32];
    if (hours > 0) {
        std::swprintf(buffer, 32, L"%lldh%02lldm%02llds", hours, minutes, seconds);
    } else if (minutes > 0) {
        std::swprintf(buffer, 32, L"%lldm%02llds", minutes, seconds);
    } else {
        std::swprintf(buffer, 32, L"%llds", seconds);
    }
    return buffer;
}

// --- Log -------------------------------------------------------------------------------

Log::Log(std::unique_ptr<util::WritableFile> file, Console& console) : file_(std::move(file)), console_(&console) {}

Log::~Log() {
    if (file_) {
        try {
            file_->finish();
        } catch (...) {
            // Nothing more can be reported at this point.
        }
    }
}

void Log::line(std::wstring_view text) {
    if (!file_) {
        return;
    }
    SYSTEMTIME now{};
    ::GetLocalTime(&now);
    wchar_t stamp[32];
    std::swprintf(stamp, 32, L"%04u-%02u-%02u %02u:%02u:%02u ", now.wYear, now.wMonth, now.wDay, now.wHour,
                  now.wMinute, now.wSecond);
    const std::string bytes = util::to_utf8(std::wstring(stamp) + std::wstring(text) + L"\n");
    try {
        file_->write(std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
    } catch (const util::FileError& error) {
        file_.reset();
        if (console_ != nullptr) {
            write_warning(*console_, L"logging stopped: " + error.message());
        }
    }
}

// --- Reporter ----------------------------------------------------------------------------

void Reporter::info(std::wstring_view text) {
    log_.line(text);
    if (text_output_ && verbosity_ != Verbosity::Quiet) {
        console_.write_out(std::wstring(text) + L"\n");
    }
}

void Reporter::detail(std::wstring_view text) {
    log_.line(text);
    if (text_output_ && verbosity_ == Verbosity::Verbose) {
        console_.write_out(std::wstring(text) + L"\n");
    }
}

void Reporter::result(std::wstring_view text) {
    log_.line(text);
    if (text_output_) {
        console_.write_out(std::wstring(text) + L"\n");
    }
}

void Reporter::warning(std::wstring_view text) {
    log_.line(L"warning: " + std::wstring(text));
    write_warning(console_, text);
}

void Reporter::error(std::wstring_view text) {
    log_.line(L"error: " + std::wstring(text));
    write_error(console_, text);
    errors_.emplace_back(text);
}

void Reporter::error_detail(std::wstring_view text) {
    log_.line(text);
    console_.write_err(std::wstring(text) + L"\n");
    errors_.emplace_back(text);
}

std::wstring labeled(std::wstring_view label, std::wstring_view value, std::size_t width) {
    std::wstring text(label);
    text.append(label.size() < width ? width - label.size() : 1, L' ');
    text += value;
    return text;
}

// --- ProgressLine ------------------------------------------------------------------------

ProgressLine::ProgressLine(Console& console, bool enabled, std::wstring label)
    : console_(console), enabled_(enabled), label_(labeled(label, L"")) {}

ProgressLine::~ProgressLine() {
    finish();
}

void ProgressLine::update(std::uint64_t done, std::uint64_t total, std::chrono::steady_clock::duration elapsed) {
    update_bytes(done * device::kSectorSize, total * device::kSectorSize, elapsed);
}

void ProgressLine::update_bytes(std::uint64_t done, std::uint64_t total, std::chrono::steady_clock::duration elapsed) {
    if (!enabled_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (drawn_ && done < total && now - last_draw_ < std::chrono::milliseconds(500)) {
        return;
    }
    last_draw_ = now;
    const double seconds = std::chrono::duration<double>(elapsed).count();
    const double rate = seconds > 0 ? static_cast<double>(done) / seconds : 0;
    const double percent = total == 0 ? 100.0 : 100.0 * static_cast<double>(done) / static_cast<double>(total);
    std::wstring eta = L"--";
    if (rate > 0 && done < total) {
        const double remaining = static_cast<double>(total - done) / rate;
        eta = format_duration(std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(remaining)));
    }
    wchar_t buffer[160];
    std::swprintf(buffer, 160, L"\r%ls%5.1f%%  %ls / %ls  %.1f MB/s  ETA %ls   ", label_.c_str(), percent,
                  format_bytes(done).c_str(), format_bytes(total).c_str(), rate / 1e6, eta.c_str());
    console_.write_err(buffer);
    drawn_ = true;
}

void ProgressLine::finish() {
    if (drawn_) {
        console_.write_err(L"\n");
        drawn_ = false;
    }
}

}  // namespace isoforge::cli
