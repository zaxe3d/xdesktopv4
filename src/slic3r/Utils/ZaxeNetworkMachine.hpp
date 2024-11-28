#pragma once

#include <string>
#include <sstream>
#include <memory>
#include <wx/versioninfo.h>
#include <wx/event.h>
#include "nlohmann/json.hpp"

#include "libslic3r/Semver.hpp"

namespace Slic3r {

struct MachineStates
{
    bool uploading_zaxe_file;
    bool calibrating;
    bool bed_occupied;
    bool bed_dirty;
    bool usb_present;
    bool preheating;
    bool printing;
    bool heating;
    bool paused;
    bool has_error;
    bool filament_present;
    bool leds_switched_on;
    bool updating_fw;

    bool is_busy() { return printing || heating || calibrating || paused || uploading_zaxe_file || updating_fw; }

    inline std::string to_string()
    {
        std::stringstream ss;
        ss << "uploading_zaxe_file: " << uploading_zaxe_file << std::endl;
        ss << "calibrating: " << calibrating << std::endl;
        ss << "bed_occupied: " << bed_occupied << std::endl;
        ss << "bed_dirty: " << bed_dirty << std::endl;
        ss << "usb_present: " << usb_present << std::endl;
        ss << "preheating: " << preheating << std::endl;
        ss << "printing: " << printing << std::endl;
        ss << "heating: " << heating << std::endl;
        ss << "paused: " << paused << std::endl;
        ss << "has_error: " << has_error << std::endl;
        ss << "filament_present: " << filament_present << std::endl;
        ss << "leds_switched_on: " << leds_switched_on << std::endl;
        return ss.str();
    }
};

struct MachineAttributes
{
    std::string serial_no;
    std::string file_name;
    std::string nozzle;
    std::string filament_color;
    std::string printing_file;
    std::string material;
    std::string material_label;
    std::string device_model;
    bool        has_pin;
    bool        has_nfc_spool;
    int         remaining_filament;
    bool        is_lite;
    bool        is_http;
    bool        is_none_TLS;
    bool        snapshot_URL;
    float       elapsed_time;
    std::string estimated_time;
    int         start_time;
    float       nozzle_temp;
    float       target_nozzle_temp;
    float       bed_temp;
    float       target_bed_temp;
    Semver      firmware_version;

    inline std::string to_string()
    {
        std::stringstream ss;
        ss << "serial_no: " << serial_no << std::endl;
        ss << "file_name: " << file_name << std::endl;
        ss << "nozzle: " << nozzle << std::endl;
        ss << "filament_color: " << filament_color << std::endl;
        ss << "printing_file: " << printing_file << std::endl;
        ss << "material: " << material << std::endl;
        ss << "material_label: " << material_label << std::endl;
        ss << "device_model: " << device_model << std::endl;
        ss << "has_pin: " << has_pin << std::endl;
        ss << "has_nfc_spool: " << has_nfc_spool << std::endl;
        ss << "remaining_filament: " << remaining_filament << std::endl;
        ss << "is_lite: " << is_lite << std::endl;
        ss << "is_http: " << is_http << std::endl;
        ss << "is_none_TLS: " << is_none_TLS << std::endl;
        ss << "snapshot_URL: " << snapshot_URL << std::endl;
        ss << "elapsed_time: " << elapsed_time << std::endl;
        ss << "estimated_time: " << estimated_time << std::endl;
        ss << "start_time: " << start_time << std::endl;
        ss << "nozzle_temp: " << nozzle_temp << std::endl;
        ss << "target_nozzle_temp: " << target_nozzle_temp << std::endl;
        ss << "bed_temp: " << bed_temp << std::endl;
        ss << "target_bed_temp: " << target_bed_temp << std::endl;
        ss << "firmware_version: " << firmware_version.to_string_sf() << std::endl;
        return ss.str();
    }
};

struct UploadProgressInfo
{
    std::string transferred_size;
    std::string total_size;
    int         progress{0};

    inline std::string to_string()
    {
        std::stringstream ss;
        ss << "progress: " << progress << std::endl;
        ss << "transferred_size: " << transferred_size << std::endl;
        ss << "total_size: " << total_size << std::endl;
        return ss.str();
    }
};

class ZaxeNetworkMachine : public std::enable_shared_from_this<ZaxeNetworkMachine>, public wxEvtHandler
{
public:
    enum class Type { LOCAL, REMOTE, UNKNOWN };

    ZaxeNetworkMachine();
    virtual ~ZaxeNetworkMachine() = default;

    virtual bool isAlive() const { return true; }

    virtual void onWSRead(const std::string& message) = 0;

    virtual void unloadFilament()                                        = 0;
    virtual void sayHi()                                                 = 0;
    virtual void togglePreheat()                                         = 0;
    virtual void toggleLeds()                                            = 0;
    virtual void cancel()                                                = 0;
    virtual void pause()                                                 = 0;
    virtual void resume()                                                = 0;
    virtual void changeName(const char* new_name)                        = 0;
    virtual void updateFirmware()                                        = 0;
    virtual void upload(const char* filename, const char* uploadAs = "") = 0;
    virtual void downloadAvatar()                                        = 0;
    virtual void switchOnCam()                                           = 0;

    bool         hasRemoteUpdate() const;
    bool         canToggleLeds() const;
    bool         hasStl() const;
    bool         hasThumbnails() const;
    virtual bool hasCam() const;
    bool         hasSnapshot() const;
    bool         canUnloadFilament() const;
    bool         canPrintMultiPlate() const;
    bool         hasPrinterCover() const;
    bool         hasFilamentPresentInfo() const;
    virtual bool canSendZaxeFile() const { return true; }

    wxBitmap& getAvatar() { return avatar; }

    std::shared_ptr<MachineStates>      states;
    std::shared_ptr<MachineAttributes>  attr;
    std::string                         name;
    std::shared_ptr<UploadProgressInfo> upload_progress_info;
    int                                 progress = 0;

protected:
    void         handle_device_message(const std::string& message);
    virtual void send(const std::string& message) = 0;

    wxBitmap avatar         = wxNullBitmap;
    bool     hello_received = false;
};

wxDECLARE_EVENT(EVT_MACHINE_OPEN, wxCommandEvent);
wxDECLARE_EVENT(EVT_MACHINE_CLOSE, wxCommandEvent);
wxDECLARE_EVENT(EVT_MACHINE_UPDATE, wxCommandEvent);
wxDECLARE_EVENT(EVT_MACHINE_SWITCH, wxCommandEvent);
wxDECLARE_EVENT(EVT_MACHINE_AVATAR_READY, wxCommandEvent);
wxDECLARE_EVENT(EVT_MACHINE_REGISTER, wxCommandEvent);
} // namespace Slic3r