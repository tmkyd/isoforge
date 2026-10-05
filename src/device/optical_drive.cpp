// SPDX-License-Identifier: Apache-2.0

#include "device/optical_drive.h"

#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

#include "device/device_error.h"
#include "util/strings.h"

namespace isoforge::device {
namespace {

constexpr ULONG kCommandTimeoutSeconds = 30;

struct PassThroughWithSense {
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG padding;  // Keeps the sense buffer aligned after the structure.
    UCHAR sense[32];
};

std::size_t page_size() {
    SYSTEM_INFO info{};
    ::GetSystemInfo(&info);
    return info.dwPageSize;
}

std::wstring descriptor_string(const std::vector<BYTE>& buffer, DWORD offset) {
    if (offset == 0 || offset >= buffer.size()) {
        return {};
    }
    std::size_t end = offset;
    while (end < buffer.size() && buffer[end] != 0) {
        ++end;
    }
    const std::wstring text = util::printable_ascii(std::span(buffer).subspan(offset, end - offset));
    const std::size_t begin = text.find_first_not_of(L' ');
    if (begin == std::wstring::npos) {
        return {};
    }
    return text.substr(begin, text.find_last_not_of(L' ') - begin + 1);
}

}  // namespace

std::size_t compute_max_transfer_bytes(std::uint32_t maximum_transfer_length,
                                       std::uint32_t maximum_physical_pages, std::size_t page_size) noexcept {
    std::size_t limit = maximum_transfer_length;
    if (maximum_physical_pages > 1 && maximum_physical_pages != std::numeric_limits<std::uint32_t>::max()) {
        limit = std::min(limit, static_cast<std::size_t>(maximum_physical_pages - 1) * page_size);
    }
    return limit;
}

OpticalDrive::OpticalDrive(util::UniqueHandle handle, std::wstring device_path)
    : handle_(std::move(handle)), device_path_(std::move(device_path)) {}

OpticalDrive::~OpticalDrive() {
    if (buffer_ != nullptr) {
        ::VirtualFree(buffer_, 0, MEM_RELEASE);
    }
}

void OpticalDrive::throw_last_error(const char* operation) const {
    throw DeviceError::from_win32(::GetLastError(), operation, util::narrow_ascii(device_path_));
}

std::unique_ptr<OpticalDrive> OpticalDrive::open(const DrivePath& drive) {
    const std::wstring path = drive.device_path();

    // A drive letter is checked before opening: opening a fixed disk volume for writing fails
    // with "access denied" for normal users, which would wrongly suggest running as administrator.
    if (drive.kind == DrivePath::Kind::DriveLetter) {
        const wchar_t root[] = {drive.letter, L':', L'\\', L'\0'};
        const UINT type = ::GetDriveTypeW(root);
        if (type == DRIVE_NO_ROOT_DIR || type == DRIVE_UNKNOWN) {
            throw DeviceError(DeviceError::Kind::NotFound, 0, util::narrow_ascii(path) + ": no such drive");
        }
        if (type != DRIVE_CDROM) {
            throw DeviceError(DeviceError::Kind::NotOpticalDrive, 0, util::narrow_ascii(path) + " is not an optical drive");
        }
    }

    util::UniqueHandle handle(::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                            FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!handle.valid()) {
        throw DeviceError::from_win32(::GetLastError(), "open", util::narrow_ascii(path));
    }
    std::unique_ptr<OpticalDrive> result(new OpticalDrive(std::move(handle), path));
    const HANDLE h = result->handle_.get();
    DWORD returned = 0;

    STORAGE_DEVICE_NUMBER number{};
    if (!::DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &number, sizeof(number), &returned,
                           nullptr)) {
        result->throw_last_error("IOCTL_STORAGE_GET_DEVICE_NUMBER on");
    }
    if (number.DeviceType != FILE_DEVICE_CD_ROM) {
        throw DeviceError(DeviceError::Kind::NotOpticalDrive, 0,
                          util::narrow_ascii(path) + " is not an optical drive (device type " +
                              std::to_string(number.DeviceType) + ")");
    }
    result->device_number_ = number.DeviceNumber;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageAdapterProperty;
    query.QueryType = PropertyStandardQuery;
    STORAGE_ADAPTER_DESCRIPTOR adapter{};
    if (!::DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &adapter, sizeof(adapter),
                           &returned, nullptr)) {
        result->throw_last_error("IOCTL_STORAGE_QUERY_PROPERTY (adapter) on");
    }
    const std::size_t page = page_size();
    if (adapter.AlignmentMask >= page) {
        throw DeviceError(DeviceError::Kind::Other, 0,
                          util::narrow_ascii(path) + ": unsupported buffer alignment mask " +
                              std::to_string(adapter.AlignmentMask));
    }
    result->limits_ = AdapterLimits{
        adapter.MaximumTransferLength, adapter.MaximumPhysicalPages, adapter.AlignmentMask,
        compute_max_transfer_bytes(adapter.MaximumTransferLength, adapter.MaximumPhysicalPages, page)};
    if (result->limits_.max_transfer_bytes < kSectorSize) {
        throw DeviceError(DeviceError::Kind::Other, 0,
                          util::narrow_ascii(path) + ": transfer limit of " +
                              std::to_string(result->limits_.max_transfer_bytes) + " bytes is below one sector");
    }
    return result;
}

StorageDeviceInfo OpticalDrive::query_device_info() {
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    STORAGE_DESCRIPTOR_HEADER header{};
    DWORD returned = 0;
    if (!::DeviceIoControl(handle_.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &header,
                           sizeof(header), &returned, nullptr)) {
        throw_last_error("IOCTL_STORAGE_QUERY_PROPERTY (device) on");
    }
    std::vector<BYTE> buffer(std::max<std::size_t>(header.Size, sizeof(STORAGE_DEVICE_DESCRIPTOR)));
    if (!::DeviceIoControl(handle_.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(),
                           static_cast<DWORD>(buffer.size()), &returned, nullptr)) {
        throw_last_error("IOCTL_STORAGE_QUERY_PROPERTY (device) on");
    }
    STORAGE_DEVICE_DESCRIPTOR descriptor{};
    std::memcpy(&descriptor, buffer.data(), sizeof(descriptor));
    return StorageDeviceInfo{descriptor_string(buffer, descriptor.VendorIdOffset),
                             descriptor_string(buffer, descriptor.ProductIdOffset),
                             descriptor_string(buffer, descriptor.ProductRevisionOffset),
                             static_cast<std::uint32_t>(descriptor.BusType)};
}

void OpticalDrive::set_media_removal_prevented(bool prevented) {
    PREVENT_MEDIA_REMOVAL request{};
    request.PreventMediaRemoval = prevented ? TRUE : FALSE;
    DWORD returned = 0;
    if (!::DeviceIoControl(handle_.get(), IOCTL_STORAGE_MEDIA_REMOVAL, &request, sizeof(request), nullptr, 0,
                           &returned, nullptr)) {
        throw_last_error("IOCTL_STORAGE_MEDIA_REMOVAL on");
    }
}

void OpticalDrive::eject() {
    DWORD returned = 0;
    if (!::DeviceIoControl(handle_.get(), IOCTL_STORAGE_EJECT_MEDIA, nullptr, 0, nullptr, 0, &returned, nullptr)) {
        throw_last_error("IOCTL_STORAGE_EJECT_MEDIA on");
    }
}

void* OpticalDrive::bounce_buffer(std::size_t size) {
    if (size > buffer_size_) {
        void* fresh = ::VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (fresh == nullptr) {
            throw_last_error("VirtualAlloc for");
        }
        if (buffer_ != nullptr) {
            ::VirtualFree(buffer_, 0, MEM_RELEASE);
        }
        buffer_ = fresh;
        buffer_size_ = size;
    }
    return buffer_;
}

ScsiResult OpticalDrive::do_execute(const Cdb& cdb, std::span<std::uint8_t> data_in) {
    void* buffer = data_in.empty() ? nullptr : bounce_buffer(data_in.size());

    PassThroughWithSense request{};
    request.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    request.sptd.CdbLength = cdb.length;
    request.sptd.SenseInfoLength = sizeof(request.sense);
    request.sptd.SenseInfoOffset = offsetof(PassThroughWithSense, sense);
    request.sptd.DataIn = data_in.empty() ? SCSI_IOCTL_DATA_UNSPECIFIED : SCSI_IOCTL_DATA_IN;
    request.sptd.DataTransferLength = static_cast<ULONG>(data_in.size());
    request.sptd.DataBuffer = buffer;
    request.sptd.TimeOutValue = kCommandTimeoutSeconds;
    std::memcpy(request.sptd.Cdb, cdb.bytes.data(), cdb.length);

    DWORD returned = 0;
    if (!::DeviceIoControl(handle_.get(), IOCTL_SCSI_PASS_THROUGH_DIRECT, &request, sizeof(request), &request,
                           sizeof(request), &returned, nullptr)) {
        throw_last_error("IOCTL_SCSI_PASS_THROUGH_DIRECT on");
    }

    ScsiResult result;
    result.status = request.sptd.ScsiStatus;
    result.transferred = std::min<std::size_t>(request.sptd.DataTransferLength, data_in.size());
    if (result.transferred > 0) {
        std::memcpy(data_in.data(), buffer, result.transferred);
    }
    const std::size_t sense_length = std::min<std::size_t>(request.sptd.SenseInfoLength, sizeof(request.sense));
    if (result.status != 0 && sense_length > 0) {
        result.sense.assign(request.sense, request.sense + sense_length);
    }
    return result;
}

}  // namespace isoforge::device
