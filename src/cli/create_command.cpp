// SPDX-License-Identifier: Apache-2.0

#include "cli/create_command.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "cli/device_errors.h"
#include "cli/exit_code.h"
#include "cli/json_values.h"
#include "cli/info_command.h"
#include "cli/reporting.h"
#include "cli/style.h"
#include "device/scsi_device.h"
#include "imaging/range_decision.h"
#include "imaging/reader.h"
#include "media/profile.h"
#include "util/json.h"
#include "util/strings.h"
#include "util/utf8.h"
#include "util/version.h"

namespace isoforge::cli {
namespace {

using util::FileError;

// What create did, for --json. Fields stay empty for steps that were not reached.
struct CreateResult {
    int exit_code = 0;
    std::vector<std::wstring> errors;  // The error messages, in order.
    std::optional<std::wstring> device;
    std::optional<std::wstring> vendor;
    std::optional<std::wstring> product;
    std::optional<std::wstring> revision;
    std::optional<media::ProfileInfo> profile;
    std::optional<imaging::SectorRange> adopted_range;
    std::optional<std::wstring> output;     // Set once the ISO has been saved under its final name.
    std::optional<std::wstring> hash_file;  // Set once the hash file has been written.
    std::optional<util::Sha256Digest> sha256;
    std::optional<std::uint64_t> sectors_written;
    std::vector<std::uint32_t> excluded;    // Sectors left out by the CD tail exception.
    std::optional<bool> verified;           // Set only with --verify.
};

// Owns <output>.partial: closes it, then deletes it unless the run succeeded (or --keep-partial).
class PartialFile {
public:
    PartialFile(util::FileSystem& files, std::wstring path, bool keep, Reporter& reporter)
        : files_(files), path_(std::move(path)), keep_(keep), reporter_(reporter) {}
    ~PartialFile() {
        file_.reset();
        if (!created_ || committed_) {
            return;
        }
        if (keep_) {
            reporter_.warning(L"the partial file was kept: " + path_);
        } else if (!files_.remove(path_)) {
            reporter_.warning(L"could not delete the partial file: " + path_);
        }
    }
    PartialFile(const PartialFile&) = delete;
    PartialFile& operator=(const PartialFile&) = delete;

    util::WritableFile& create() {
        file_ = files_.create(path_, util::CreateMode::Replace);
        created_ = true;
        return *file_;
    }
    void finish() {
        file_->finish();
        file_.reset();
    }
    void commit() { committed_ = true; }

private:
    util::FileSystem& files_;
    std::wstring path_;
    bool keep_;
    Reporter& reporter_;
    std::unique_ptr<util::WritableFile> file_;
    bool created_ = false;
    bool committed_ = false;
};

class FileSink final : public imaging::DataSink {
public:
    explicit FileSink(util::WritableFile& file) : file_(file) {}
    void write(std::span<const std::uint8_t> data) override { file_.write(data); }

private:
    util::WritableFile& file_;
};

class NullSink final : public imaging::DataSink {
public:
    void write(std::span<const std::uint8_t>) override {}
};

class CreateObserver final : public imaging::ReadObserver {
public:
    // `pass` names the pass in the log ("read" or "verify").
    CreateObserver(Reporter& reporter, ProgressLine& progress, const std::function<bool()>& interrupted,
                   std::wstring pass)
        : reporter_(reporter), progress_(progress), interrupted_(interrupted), pass_(std::move(pass)) {}

    void on_progress(const imaging::ReadProgress& p) override {
        progress_.update(p.sectors_done, p.sectors_total, p.elapsed);
        log_progress(p);
    }
    // Read failures, with the sense data, go to the log and to -v output.
    void on_range_failure(std::uint32_t lba, std::uint32_t sectors, const device::ScsiCommandError& error) override {
        progress_.finish();
        reporter_.detail(L"read of " + std::to_wstring(sectors) + L" sectors at " + std::to_wstring(lba) +
                         L" failed; retrying sector by sector: " + util::widen_ascii(error.what()));
    }
    void on_read_failure(std::uint32_t lba, std::uint64_t attempt, const device::ScsiCommandError& error) override {
        progress_.finish();
        reporter_.detail(L"read failed at sector " + std::to_wstring(lba) + L" (attempt " + std::to_wstring(attempt) +
                         L"): " + util::widen_ascii(error.what()));
    }
    bool cancel_requested() override { return interrupted_ && interrupted_(); }

private:
    // Every 10% of the pass goes to the log.
    void log_progress(const imaging::ReadProgress& p) {
        if (p.sectors_total == 0) {
            return;
        }
        const std::uint64_t percent = p.sectors_done * 100 / p.sectors_total;
        while (next_percent_ <= 100 && percent >= next_percent_) {
            const double seconds = std::chrono::duration<double>(p.elapsed).count();
            const double rate = seconds > 0 ? static_cast<double>(p.sectors_done) * device::kSectorSize / seconds : 0;
            wchar_t speed[32];
            std::swprintf(speed, 32, L"%.1f MB/s", rate / 1e6);
            reporter_.log_only(pass_ + L" progress: " + std::to_wstring(next_percent_) + L"% (" +
                               std::to_wstring(p.sectors_done) + L" of " + std::to_wstring(p.sectors_total) +
                               L" sectors, " + speed + L")");
            next_percent_ += 10;
        }
    }

    Reporter& reporter_;
    ProgressLine& progress_;
    const std::function<bool()>& interrupted_;
    std::wstring pass_;
    std::uint64_t next_percent_ = 10;
};

// The files create writes, as full paths.
struct OutputPaths {
    std::wstring output;
    std::wstring partial;                   // <output>.partial while reading.
    std::optional<std::wstring> hash_file;  // Without --no-hash-file.
    std::wstring directory;                 // Empty when the output has no parent directory.
};

OutputPaths resolve_outputs(const CreateOptions& options, util::FileSystem& files) {
    OutputPaths paths;
    paths.output = files.full_path(options.output);
    paths.partial = paths.output + L".partial";
    if (options.hash_file) {
        paths.hash_file = files.full_path(*options.hash_file);
    }
    paths.directory = util::parent_directory(paths.output);
    return paths;
}

// Checks the output paths before the disc is touched. Returns the exit code when create must stop.
std::optional<int> check_outputs(const OutputPaths& paths, bool overwrite, util::FileSystem& files,
                                 Reporter& reporter) {
    if (paths.hash_file) {
        // The hash file is written through <hash file>.partial after the ISO is renamed, so
        // neither path may be the ISO.
        if (util::equals_ignore_case(*paths.hash_file, paths.output) ||
            util::equals_ignore_case(*paths.hash_file + L".partial", paths.output)) {
            reporter.error(L"the hash file and its temporary file must differ from the output file: " + paths.output);
            return to_int(ExitCode::UsageError);
        }
    }
    if (!paths.directory.empty() && !files.directory_exists(paths.directory)) {
        reporter.error(L"the output directory does not exist: " + paths.directory);
        return to_int(ExitCode::OutputError);
    }
    if (!overwrite) {
        std::vector<std::wstring> targets = {paths.output};
        if (paths.hash_file) {
            targets.push_back(*paths.hash_file);
        }
        for (const std::wstring& path : targets) {
            if (files.exists(path)) {
                reporter.error(L"the file already exists (use --overwrite to replace it): " + path);
                return to_int(ExitCode::OutputError);
            }
        }
    }
    return std::nullopt;
}

// Checks that the output volume can store `bytes`. Returns the exit code when it cannot.
std::optional<int> check_space(util::FileSystem& files, const OutputPaths& paths, std::uint64_t bytes,
                               Reporter& reporter) {
    const util::VolumeInfo volume = files.volume_info(paths.directory.empty() ? paths.output : paths.directory);
    if (volume.max_file_size && bytes > *volume.max_file_size) {
        reporter.error(L"the " + volume.filesystem + L" file system cannot store a file of " + std::to_wstring(bytes) +
                       L" bytes (limit " + std::to_wstring(*volume.max_file_size) + L" bytes)");
        return to_int(ExitCode::OutputError);
    }
    // A stale partial file is truncated when the new one is created, so its space counts as
    // free. An existing output replaced with --overwrite is removed only at the final rename,
    // so its space is not available while reading.
    const std::uint64_t available = volume.free_bytes + files.file_size(paths.partial).value_or(0);
    if (available < bytes) {
        reporter.error(L"not enough free space: " + format_bytes(bytes) + L" needed, " + format_bytes(available) +
                       L" available");
        return to_int(ExitCode::OutputError);
    }
    return std::nullopt;
}

// Writes the hash file through <hash file>.partial. Returns the exit code on failure.
std::optional<int> write_hash_file(util::FileSystem& files, const std::wstring& hash_file,
                                   const util::Sha256Digest& digest, const std::wstring& iso_path, bool overwrite,
                                   Reporter& reporter) {
    const std::wstring temporary = hash_file + L".partial";
    try {
        const std::string line = util::to_utf8(hash_file_line(digest, iso_path));
        std::unique_ptr<util::WritableFile> hash = files.create(temporary, util::CreateMode::Replace);
        hash->write(std::span(reinterpret_cast<const std::uint8_t*>(line.data()), line.size()));
        hash->finish();
        hash.reset();
        files.rename(temporary, hash_file, overwrite);
    } catch (const FileError& error) {
        files.remove(temporary);
        reporter.error(L"the ISO was saved, but the hash file could not be written: " + error.message());
        return to_int(ExitCode::OutputError);
    }
    return std::nullopt;
}

// Writes the drive and disc details of info to the log.
void log_details(Reporter& reporter, const InfoReport& details) {
    const std::wstring text = format_info_text(details);
    for (std::size_t begin = 0; begin < text.size();) {
        const std::size_t end = text.find(L'\n', begin);
        reporter.log_only(L"  " + text.substr(begin, end - begin));
        begin = end == std::wstring::npos ? text.size() : end + 1;
    }
}

// Everything from the output checks to the hash file; fills `result` as it goes.
int create_disc(const CreateOptions& options, Console& console, const Services& services, Reporter& reporter,
                CreateResult& result) {
    util::FileSystem& files = services.files;
    try {
        const OutputPaths paths = resolve_outputs(options, files);
        if (const auto code = check_outputs(paths, options.overwrite, files, reporter)) {
            return *code;
        }

        // --- Disc ----------------------------------------------------------------------------
        OpenedDrive drive = services.drives.open(options.drive);
        device::ScsiDevice scsi(*drive.transport);
        result.device = drive.device_path;
        result.vendor = drive.vendor;
        result.product = drive.product;
        result.revision = drive.revision;
        reporter.info(labeled(L"Drive:", drive.device_path + L" (" +
                                             format_model(drive.vendor, drive.product, drive.revision) + L")"));
        if (const auto problem = wait_for_disc(scsi, drive.device_path, services, options.wait_seconds)) {
            reporter.error(*problem);
            return to_int(ExitCode::Unsupported);
        }
        const InfoReport details = inspect_disc(scsi, drive);
        result.profile = details.media.profile;
        log_details(reporter, details);
        if (!details.supported()) {
            reporter.error(L"the disc is not supported:");
            for (const std::wstring& reason : details.reasons()) {
                reporter.error_detail(L"  - " + reason);
            }
            return to_int(ExitCode::Unsupported);
        }
        const imaging::SectorRange range = *details.range->adopted;
        const media::ProfileInfo& profile = *details.media.profile;
        result.adopted_range = range;
        reporter.info(labeled(L"Disc:", profile.name + L", " + format_range(range)));
        reporter.info(labeled(L"Output:", paths.output));
        if (const auto code = check_space(files, paths, range.bytes(), reporter)) {
            return *code;
        }

        // --- Read the disc into the partial file -----------------------------------------------
        PartialFile guard(files, paths.partial, options.keep_partial, reporter);
        util::WritableFile& file = guard.create();

        imaging::ReadOptions read_options;
        read_options.retries = options.retries;
        read_options.cd_tail_exception = profile.family == media::MediaFamily::Cd;
        read_options.filesystem_end = details.range->filesystem_end.value_or(range.end());
        read_options.prevent_media_removal = drive.prevent_media_removal;
        read_options.sleep = [&](std::chrono::milliseconds d) { services.drives.sleep(d); };

        // One pass over the disc with a progress line; `pass` names it in the log.
        const bool show_progress =
            options.progress && options.verbosity != Verbosity::Quiet && console.err_is_terminal();
        const auto read_pass = [&](const wchar_t* label, const wchar_t* pass, const imaging::SectorRange& sectors,
                                   const imaging::ReadOptions& pass_options, imaging::DataSink& sink) {
            ProgressLine progress(console, show_progress, label);
            CreateObserver observer(reporter, progress, services.interrupted, pass);
            return imaging::read_range(scsi, sectors, pass_options, sink, observer);
        };

        imaging::ReadResult first;
        {
            FileSink sink(file);
            first = read_pass(L"Reading:", L"read", range, read_options, sink);
        }
        if (first.outcome == imaging::ReadOutcome::Cancelled) {
            reporter.error(L"interrupted");
            return to_int(ExitCode::Interrupted);
        }
        if (first.outcome == imaging::ReadOutcome::ReadError) {
            reporter.error(first.error);
            return to_int(ExitCode::ReadError);
        }
        guard.finish();
        result.sha256 = first.sha256;
        result.sectors_written = first.sectors_written;
        result.excluded = first.excluded;
        reporter.info(labeled(L"Read:", std::to_wstring(first.sectors_written) + L" sectors"));
        if (!first.excluded.empty()) {
            std::wstring lbas;
            for (const std::uint32_t lba : first.excluded) {
                lbas += (lbas.empty() ? L"" : L", ") + std::to_wstring(lba);
            }
            reporter.warning(L"unreadable sectors at the end of the CD track were excluded (LBA " + lbas + L"; " +
                             first.excluded_reason + L"); the ISO has " +
                             std::to_wstring(first.sectors_written) + L" sectors");
        }

        // --- --verify: read the disc again and compare ------------------------------------------
        if (options.verify) {
            imaging::ReadOptions verify_options = read_options;
            verify_options.cd_tail_exception = false;
            NullSink null_sink;
            const imaging::ReadResult second =
                read_pass(L"Verifying:", L"verify",
                          imaging::SectorRange{range.start, static_cast<std::uint32_t>(first.sectors_written)},
                          verify_options, null_sink);
            if (second.outcome == imaging::ReadOutcome::Cancelled) {
                reporter.error(L"interrupted");
                return to_int(ExitCode::Interrupted);
            }
            if (second.outcome == imaging::ReadOutcome::ReadError) {
                result.verified = false;
                reporter.error(L"verification failed: " + second.error);
                return to_int(ExitCode::ReadError);
            }
            if (*second.sha256 != *first.sha256) {
                result.verified = false;
                reporter.error(L"verification failed: the second read gave SHA-256 " + util::to_hex(*second.sha256) +
                               L", the first " + util::to_hex(*first.sha256));
                return to_int(ExitCode::HashMismatch);
            }
            result.verified = true;
            reporter.info(labeled(L"Verified:", L"the second read matches"));
        }

        // --- Commit ---------------------------------------------------------------------------
        files.rename(paths.partial, paths.output, options.overwrite);
        guard.commit();
        result.output = paths.output;
        reporter.result(labeled(L"SHA-256:", util::to_hex(*first.sha256)));
        reporter.info(labeled(L"ISO:", paths.output));

        if (paths.hash_file) {
            if (const auto code = write_hash_file(files, *paths.hash_file, *first.sha256, paths.output,
                                                  options.overwrite, reporter)) {
                return *code;
            }
            result.hash_file = *paths.hash_file;
            reporter.info(labeled(L"Hash file:", *paths.hash_file));
        }

        if (options.eject && drive.eject) {
            try {
                drive.eject();
            } catch (const device::DeviceError& error) {
                reporter.warning(L"could not eject the disc: " + util::widen_ascii(error.what()));
            }
        }
        reporter.log_only(L"completed");
        return to_int(ExitCode::Success);
    } catch (const Interrupted&) {
        reporter.error(L"interrupted");
        return to_int(ExitCode::Interrupted);
    } catch (const FileError& error) {
        reporter.error(error.message());
        return to_int(ExitCode::OutputError);
    } catch (const device::DeviceError& error) {
        reporter.error(describe(error));
        return to_int(exit_code_for(error));
    } catch (const std::exception& error) {
        // Reported here, not in run(), so that it also reaches the --log file.
        reporter.error(L"internal error: " + util::widen_ascii(error.what()));
        return to_int(ExitCode::OtherError);
    }
}

}  // namespace

std::wstring hash_file_line(const util::Sha256Digest& digest, const std::wstring& iso_path) {
    return util::to_hex(digest) + L" *" + util::file_name(iso_path) + L"\n";
}

namespace {

std::wstring format_create_json(const CreateResult& r) {
    std::wstring error;
    for (const std::wstring& line : r.errors) {
        error += (error.empty() ? L"" : L"\n") + line;
    }
    std::wstring text = L"{\n";
    text += L"  \"exit_code\": " + std::to_wstring(r.exit_code) + L",\n";
    text += L"  \"success\": " + std::wstring(r.exit_code == 0 ? L"true" : L"false") + L",\n";
    text += L"  \"error\": " + (r.errors.empty() ? std::wstring(L"null") : util::json_string(error)) + L",\n";
    text += L"  \"device\": " + json_or_null(r.device) + L",\n";
    text += L"  \"vendor\": " + json_or_null(r.vendor) + L",\n";
    text += L"  \"product\": " + json_or_null(r.product) + L",\n";
    text += L"  \"revision\": " + json_or_null(r.revision) + L",\n";
    text += L"  \"profile\": " + profile_json(r.profile) + L",\n";
    text += L"  \"adopted_range\": " + range_json(r.adopted_range) + L",\n";
    text += L"  \"output\": " + json_or_null(r.output) + L",\n";
    text += L"  \"hash_file\": " + json_or_null(r.hash_file) + L",\n";
    text += L"  \"sha256\": " + (r.sha256 ? util::json_string(util::to_hex(*r.sha256)) : std::wstring(L"null")) + L",\n";
    text += L"  \"sectors_written\": " +
            (r.sectors_written ? std::to_wstring(*r.sectors_written) : std::wstring(L"null")) + L",\n";
    text += L"  \"excluded_sectors\": [";
    for (std::size_t i = 0; i < r.excluded.size(); ++i) {
        text += (i == 0 ? L"" : L", ") + std::to_wstring(r.excluded[i]);
    }
    text += L"],\n  \"verified\": ";
    text += r.verified ? (*r.verified ? L"true" : L"false") : L"null";
    text += L"\n}\n";
    return text;
}

}  // namespace

int run_create(const CreateOptions& options, Console& console, const Services& services,
               std::wstring_view command_line) {
    CreateResult result;
    Log log;
    bool log_ready = true;
    if (options.log_file) {
        try {
            log = Log(services.files.create(services.files.full_path(*options.log_file), util::CreateMode::Append),
                      console);
        } catch (const FileError& error) {
            write_error(console, error.message());
            result.errors.push_back(error.message());
            result.exit_code = to_int(ExitCode::OutputError);
            log_ready = false;
        }
    }
    if (log_ready) {
        Reporter reporter(console, options.verbosity, log, !options.json);
        reporter.log_only(L"isoforge " + std::wstring(version()) + L": " + std::wstring(command_line));
        result.exit_code = create_disc(options, console, services, reporter, result);
        result.errors = reporter.errors();
    }
    if (options.json) {
        console.write_out(format_create_json(result));
    }
    return result.exit_code;
}

}  // namespace isoforge::cli
