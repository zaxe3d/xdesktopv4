#pragma once

#include "libslic3r/Semver.hpp"
#include "NetworkMachine.hpp"

namespace Slic3r {
class ZaxeDeviceCapabilities
{
public:
    enum class TransferType { HTTP, HTTPS, FTP };

    ZaxeDeviceCapabilities(NetworkMachine* _nm);

    bool         hasRemoteUpdate() const;
    bool         canToggleLeds() const;
    bool         hasStl() const;
    bool         hasThumbnails() const;
    bool         hasCam() const;
    bool         hasSnapshot() const;
    bool         canUnloadFilament() const;
    bool         canPrintMultiPlate() const;
    bool         hasPrinterCover() const;
    TransferType getUploadType() const;
    TransferType getSnapshotDownloadType() const;
    bool         can_set_bed_state() const;

    Semver get_version() const { return version; }

private:
    NetworkMachine* nm;
    Semver          version;
};
} // namespace Slic3r