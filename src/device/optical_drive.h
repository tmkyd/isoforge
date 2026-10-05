// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "device/drive_path.h"
#include "device/scsi_transport.h"
#include "util/unique_handle.h"

namespace isoforge::device {

// Values of STORAGE_ADAPTER_DESCRIPTOR and the transfer limit derived from them.
struct AdapterLimits {
    std::uint32_t maximum_transfer_length = 0;
    std::uint32_t maximum_physical_pages = 0;
    std::uint32_t alignment_mask = 0;
    std::size_t max_transfer_bytes = 0;
};

// Values of STORAGE_DEVICE_DESCRIPTOR.
struct StorageDeviceInfo {
    std::wstring vendor;
    std::wstring product;
    std::wstring revision;
    std::uint32_t bus_type = 0;  // STORAGE_BUS_TYPE.
};

// Computes the per-command transfer limit: MaximumTransferLength, and at most
// (MaximumPhysicalPages - 1) pages so that a buffer spanning an extra page still fits.
std::size_t compute_max_transfer_bytes(std::uint32_t maximum_transfer_length,
                                       std::uint32_t maximum_physical_pages, std::size_t page_size) noexcept;

// An optical drive opened through its Win32 device name. SCSI commands go through SPTI
// (IOCTL_SCSI_PASS_THROUGH_DIRECT); ReadFile is never used on the device.
class OpticalDrive final : public ScsiTransport {
public:
    // Opens the drive with GENERIC_READ | GENERIC_WRITE (required by SPTI) and checks that it is
    // FILE_DEVICE_CD_ROM. Throws DeviceError.
    static std::unique_ptr<OpticalDrive> open(const DrivePath& drive);

    ~OpticalDrive() override;
    OpticalDrive(const OpticalDrive&) = delete;
    OpticalDrive& operator=(const OpticalDrive&) = delete;

    const std::wstring& device_path() const noexcept { return device_path_; }
    std::uint32_t device_number() const noexcept { return device_number_; }
    const AdapterLimits& adapter_limits() const noexcept { return limits_; }

    // IOCTL_STORAGE_QUERY_PROPERTY (StorageDeviceProperty). Throws DeviceError.
    StorageDeviceInfo query_device_info();

    // IOCTL_STORAGE_MEDIA_REMOVAL. The class driver releases the lock when the handle is closed.
    void set_media_removal_prevented(bool prevented);

    // IOCTL_STORAGE_EJECT_MEDIA.
    void eject();

    std::size_t max_transfer_bytes() const override { return limits_.max_transfer_bytes; }

private:
    OpticalDrive(util::UniqueHandle handle, std::wstring device_path);

    ScsiResult do_execute(const Cdb& cdb, std::span<std::uint8_t> data_in) override;
    [[noreturn]] void throw_last_error(const char* operation) const;
    void* bounce_buffer(std::size_t size);

    util::UniqueHandle handle_;
    std::wstring device_path_;
    std::uint32_t device_number_ = 0;
    AdapterLimits limits_;
    void* buffer_ = nullptr;  // Page-aligned (VirtualAlloc) so that it meets the alignment mask.
    std::size_t buffer_size_ = 0;
};

}  // namespace isoforge::device
