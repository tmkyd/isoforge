// SPDX-License-Identifier: Apache-2.0

#include "cli/drive_provider.h"

#include <windows.h>

#include "device/optical_drive.h"
#include "device/scsi_device.h"
#include "media/assessment.h"

namespace isoforge::cli {

std::vector<device::DriveEntry> SystemDriveProvider::enumerate() {
    return device::enumerate_optical_drives();
}

DriveReport SystemDriveProvider::probe(const device::DriveEntry& entry) {
    DriveReport report{entry, {}, {}, {}, std::nullopt, std::nullopt};
    try {
        const auto drive =
            device::OpticalDrive::open(device::DrivePath{device::DrivePath::Kind::CdRom, L'\0', entry.number});
        const device::StorageDeviceInfo info = drive->query_device_info();
        report.vendor = info.vendor;
        report.product = info.product;
        report.revision = info.revision;
        device::ScsiDevice scsi(*drive);
        report.media = media::probe_media(scsi);
        if (report.media->state == media::MediaState::Loaded) {
            // The disc type is informative only: failing to read it is not an error for list.
            try {
                report.profile = media::read_current_profile(scsi);
            } catch (const device::ScsiError&) {
            }
        }
    } catch (const device::DeviceError& e) {
        report.error = e;
    }
    return report;
}

OpenedDrive SystemDriveProvider::open(const device::DrivePath& drive) {
    std::unique_ptr<device::OpticalDrive> optical = device::OpticalDrive::open(drive);
    const device::StorageDeviceInfo info = optical->query_device_info();
    OpenedDrive opened;
    opened.device_path = optical->device_path();
    opened.device_number = optical->device_number();
    opened.vendor = info.vendor;
    opened.product = info.product;
    opened.revision = info.revision;
    // The transport owns the drive, so these stay valid as long as the OpenedDrive.
    device::OpticalDrive* raw = optical.get();
    opened.prevent_media_removal = [raw](bool prevent) { raw->set_media_removal_prevented(prevent); };
    opened.eject = [raw] { raw->eject(); };
    opened.transport = std::move(optical);
    return opened;
}

void SystemDriveProvider::sleep(std::chrono::milliseconds duration) {
    ::Sleep(static_cast<DWORD>(duration.count()));
}

}  // namespace isoforge::cli
