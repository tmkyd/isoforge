// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "device/device_error.h"
#include "device/drive_enumeration.h"
#include "device/drive_path.h"
#include "device/scsi_transport.h"
#include "media/profile.h"
#include "media/readiness.h"

namespace isoforge::cli {

// What list shows for one drive.
struct DriveReport {
    device::DriveEntry entry;
    std::wstring vendor;
    std::wstring product;
    std::wstring revision;
    std::optional<media::MediaStatus> media;    // Empty when the drive could not be probed.
    std::optional<device::DeviceError> error;   // Why the drive could not be opened or probed.
    std::optional<media::ProfileInfo> profile;  // Disc type when a disc is loaded and it could be read.
};

// A drive opened for info or create.
struct OpenedDrive {
    std::unique_ptr<device::ScsiTransport> transport;
    std::wstring device_path;                   // e.g. L"\\\\.\\Z:".
    std::optional<std::uint32_t> device_number;  // N of CdRomN.
    std::wstring vendor;
    std::wstring product;
    std::wstring revision;
    // IOCTL_STORAGE_MEDIA_REMOVAL and IOCTL_STORAGE_EJECT_MEDIA; empty when not available.
    std::function<void(bool)> prevent_media_removal;
    std::function<void()> eject;
};

// Access to drives and time; tests substitute fakes.
class DriveProvider {
public:
    virtual ~DriveProvider() = default;
    virtual std::vector<device::DriveEntry> enumerate() = 0;
    virtual DriveReport probe(const device::DriveEntry& entry) = 0;
    // Throws device::DeviceError.
    virtual OpenedDrive open(const device::DrivePath& drive) = 0;
    virtual void sleep(std::chrono::milliseconds duration) = 0;
};

// Real drives: QueryDosDeviceW, OpticalDrive (SPTI), STORAGE_DEVICE_DESCRIPTOR and Sleep.
class SystemDriveProvider final : public DriveProvider {
public:
    std::vector<device::DriveEntry> enumerate() override;
    DriveReport probe(const device::DriveEntry& entry) override;
    OpenedDrive open(const device::DrivePath& drive) override;
    void sleep(std::chrono::milliseconds duration) override;
};

}  // namespace isoforge::cli
