#include "ZaxeDeviceCapabilities.hpp"
#include "libslic3r/Utils.hpp"

namespace Slic3r {
ZaxeDeviceCapabilities::ZaxeDeviceCapabilities(NetworkMachine* _nm)
    : nm(_nm)
    , version(Semver(nm->attr->firmware_version.GetMajor(), nm->attr->firmware_version.GetMinor(), nm->attr->firmware_version.GetMicro()))
{}

bool ZaxeDeviceCapabilities::hasRemoteUpdate() const { return is_there(nm->attr->device_model, {"z3", "z4", "x4"}); }

bool ZaxeDeviceCapabilities::canToggleLeds() const
{
    return is_there(nm->attr->device_model, {"z3", "z4", "x4"}) && version >= Semver(3, 5, 70);
}

bool ZaxeDeviceCapabilities::hasStl() const { return is_there(nm->attr->device_model, {"z2", "z3", "z4", "x4"}); }

bool ZaxeDeviceCapabilities::hasThumbnails() const { return is_there(nm->attr->device_model, {"z1", "z2", "z3", "z4", "x4"}); }

bool ZaxeDeviceCapabilities::hasCam() const { return is_there(nm->attr->device_model, {"z2", "z3", "z4", "x4"}); }

bool ZaxeDeviceCapabilities::hasSnapshot() const { return is_there(nm->attr->device_model, {"z1", "z2", "z3", "z4", "x4"}); }

bool ZaxeDeviceCapabilities::canUnloadFilament() const { return is_there(nm->attr->device_model, {"z1", "z2", "z3", "z4", "x4"}); }

bool ZaxeDeviceCapabilities::canPrintMultiPlate() const
{
    return is_there(nm->attr->device_model, {"z3", "z4", "x4"}) && version >= Semver(3, 5, 78);
}

bool ZaxeDeviceCapabilities::hasPrinterCover() const { return is_there(nm->attr->device_model, {"z1", "z3", "x1", "x2", "x3", "x4"}); };

ZaxeDeviceCapabilities::TransferType ZaxeDeviceCapabilities::getUploadType() const
{
    if (nm->attr->is_http) {
        return TransferType::HTTP;
    } else if (version >= Semver(5, 0, 0)) {
        return TransferType::HTTPS;
    }
    return TransferType::FTP;
}

ZaxeDeviceCapabilities::TransferType ZaxeDeviceCapabilities::getSnapshotDownloadType() const
{
    if (version >= Semver(5, 0, 0)) {
        return TransferType::HTTPS;
    }
    return TransferType::FTP;
}

bool ZaxeDeviceCapabilities::can_set_bed_state() const { return version >= Semver(5, 0, 0); }
} // namespace Slic3r