// SPDX-License-Identifier: Apache-2.0

#include "cli/verify_command.h"

#include <chrono>
#include <vector>

#include "cli/exit_code.h"
#include "cli/reporting.h"
#include "util/strings.h"
#include "util/utf8.h"

namespace isoforge::cli {
namespace {

int hex_value(wchar_t c) {
    if (c >= L'0' && c <= L'9') {
        return c - L'0';
    }
    if (c >= L'a' && c <= L'f') {
        return c - L'a' + 10;
    }
    if (c >= L'A' && c <= L'F') {
        return c - L'A' + 10;
    }
    return -1;
}

constexpr std::size_t kReadChunk = 1 << 20;
constexpr std::size_t kMaxHashFileSize = 64 * 1024;

}  // namespace

std::optional<util::Sha256Digest> parse_digest(std::wstring_view text) {
    util::Sha256Digest digest{};
    if (text.size() != digest.size() * 2) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < digest.size(); ++i) {
        const int high = hex_value(text[2 * i]);
        const int low = hex_value(text[2 * i + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        digest[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return digest;
}

std::optional<HashFileEntry> parse_hash_file(std::string_view content) {
    if (content.starts_with("\xEF\xBB\xBF")) {
        content.remove_prefix(3);
    }
    while (!content.empty()) {
        const std::size_t end = content.find('\n');
        std::string_view line = content.substr(0, end);
        content = end == std::string_view::npos ? std::string_view{} : content.substr(end + 1);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.remove_suffix(1);
        }
        if (line.empty()) {
            continue;
        }
        if (line.size() < 64 || (line.size() > 64 && line[64] != ' ' && line[64] != '\t')) {
            return std::nullopt;
        }
        const std::optional<util::Sha256Digest> digest = parse_digest(std::wstring(line.begin(), line.begin() + 64));
        if (!digest) {
            return std::nullopt;
        }
        HashFileEntry entry{*digest, {}};
        if (line.size() > 65) {
            // sha256sum writes a mode character after the separator: '*' (binary) or ' ' (text).
            std::string_view name = line.substr(65);
            if (!name.empty() && (name.front() == '*' || name.front() == ' ')) {
                name.remove_prefix(1);
            }
            entry.file_name = util::from_utf8(name);
        }
        return entry;
    }
    return std::nullopt;
}

int run_verify(const VerifyOptions& options, Console& console, const Services& services) {
    util::FileSystem& files = services.files;
    Log log;
    Reporter reporter(console, options.quiet ? Verbosity::Quiet : Verbosity::Normal, log);
    try {
        const std::wstring iso = files.full_path(options.iso_file);
        std::optional<util::Sha256Digest> expected = parse_digest(options.hash);
        if (!expected) {
            const std::wstring hash_path = files.full_path(options.hash);
            std::unique_ptr<util::ReadableFile> hash_file = files.open(hash_path);
            std::string content;
            std::vector<std::uint8_t> buffer(4096);
            while (const std::size_t got = hash_file->read(buffer)) {
                content.append(reinterpret_cast<const char*>(buffer.data()), got);
                if (content.size() > kMaxHashFileSize) {
                    break;
                }
            }
            const std::optional<HashFileEntry> entry = parse_hash_file(content);
            if (!entry) {
                reporter.error(L"the hash file does not start with a SHA-256 in sha256sum format: " + hash_path);
                return to_int(ExitCode::OutputError);
            }
            expected = entry->digest;
            // A hash file that names another file is probably the wrong one; the hash decides.
            if (!entry->file_name.empty() && !util::equals_ignore_case(entry->file_name, util::file_name(iso))) {
                reporter.warning(L"the hash file is for '" + entry->file_name + L"', not '" + util::file_name(iso) +
                                 L"'");
            }
        }

        std::unique_ptr<util::ReadableFile> file = files.open(iso);
        const std::uint64_t size = files.file_size(iso).value_or(0);
        util::Sha256 sha;
        std::vector<std::uint8_t> buffer(kReadChunk);
        std::uint64_t total = 0;
        const auto started = std::chrono::steady_clock::now();
        {
            ProgressLine progress(console, options.progress && !options.quiet && console.err_is_terminal(),
                                  L"Hashing:");
            while (const std::size_t got = file->read(buffer)) {
                sha.update(std::span(buffer).first(got));
                total += got;
                progress.update_bytes(total, size, std::chrono::steady_clock::now() - started);
                if (services.interrupted && services.interrupted()) {
                    progress.finish();
                    reporter.error(L"interrupted");
                    return to_int(ExitCode::Interrupted);
                }
            }
        }
        const util::Sha256Digest actual = sha.finish();

        reporter.info(labeled(L"File:", iso + L" (" + std::to_wstring(total) + L" bytes)"));
        reporter.info(labeled(L"SHA-256:", util::to_hex(actual)));
        reporter.info(labeled(L"Expected:", util::to_hex(*expected)));
        if (actual != *expected) {
            reporter.result(labeled(L"Result:", L"MISMATCH"));
            return to_int(ExitCode::HashMismatch);
        }
        reporter.result(labeled(L"Result:", L"OK"));
        return to_int(ExitCode::Success);
    } catch (const util::FileError& error) {
        reporter.error(error.message());
        return to_int(ExitCode::OutputError);
    }
}

}  // namespace isoforge::cli
