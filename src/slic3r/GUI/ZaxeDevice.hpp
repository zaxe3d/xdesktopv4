#pragma once

#include <wx/panel.h>
#include <wx/statbmp.h>

#include "Widgets/Label.hpp"
#include "Widgets/ProgressBar.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/SwitchButton.hpp"

#include "../Utils/NetworkMachine.hpp"
#include "I18N.hpp"
#include "libslic3r/Semver.hpp"

#include <optional>

namespace Slic3r::GUI {

class ZaxeDeviceCapabilities
{
public:
    ZaxeDeviceCapabilities(NetworkMachine* _nm);

    bool hasRemoteUpdate();
    bool canToggleLeds();
    bool hasStl();
    bool hasThumbnails();

private:
    NetworkMachine* nm;
    Semver          version;
};

class ZaxeDevice : public wxPanel
{
public:
    ZaxeDevice(NetworkMachine* _nm, wxWindow* parent, wxPoint pos = wxDefaultPosition, wxSize size = wxDefaultSize);
    ~ZaxeDevice();

    void updateStates();
    void updateProgressValue();
    void enablePrintButton(bool enable_for_all, bool enable_for_current_plate);

    void onPrintDenied();
    void onAvatarReady();
    void onTemperatureUpdate();
    void onUploadDone();
    void onVersionCheck(const std::map<std::string, Semver>& latest_versions);

    bool     isBusy();
    void     setName(const string& name);
    wxString getName() const;

    void setMaterialLabel(const std::string& material_label);
    void setFilamentPresent(bool present);
    void setNozzle(const std::string& nozzle);
    void setPin(bool has_pin);
    void setFileStart();

    bool has(const wxString& search_text);

    bool print();

private:
    NetworkMachine* nm;
    wxTimer*        timer;

    Button*           model_btn{nullptr};
    Button*           model_btn_expanded{nullptr};
    Label*            device_name{nullptr};
    wxTextCtrl*       device_name_ctrl{nullptr};
    Button*           expand_btn{nullptr};
    wxBitmap          default_avatar;
    wxStaticBitmap*   avatar{nullptr};
    RoundedRectangle* avatar_rect{nullptr};
    Label*            status_title{nullptr};
    Label*            status_desc{nullptr};
    Button*           status_desc_icon{nullptr};
    Button*           print_btn{nullptr};
    SwitchButton*     print_mode{nullptr};
    ProgressBar*      progress_bar{nullptr};
    Label*            progress_label{nullptr};
    wxPanel*          progress_line{nullptr};
    Button*           pause_btn{nullptr};
    Button*           resume_btn{nullptr};
    Button*           stop_btn{nullptr};
    Button*           preheat_btn{nullptr};
    Button*           say_hi_btn{nullptr};
    Button*           unload_btn{nullptr};
    Button*           toggle_leds_btn{nullptr};
    Label*            material_val{nullptr};
    Label*            nozzle_val{nullptr};
    Label*            nozzle_temp_val{nullptr};
    Label*            plate_temp_val{nullptr};
    Label*            printing_file{nullptr};
    Label*            printing_file_val{nullptr};
    Label*            printing_time{nullptr};
    Label*            printing_time_val{nullptr};
    wxSizer*          detailed_info_sizer{nullptr};
    Label*            version{nullptr};

    bool device_name_ctrl_visible{false};
    bool is_expanded{false};
    bool is_file_name_visible{false};

    ZaxeDeviceCapabilities capabilities;
    std::optional<Semver>  upstream_version;
    bool                   update_available{false};

    bool print_enable_for_current_plate{false};
    bool print_enable_for_all{false};

    void     onTimer(wxTimerEvent& event);
    wxSizer* createHeader();
    void     createAvatar();
    wxSizer* createStateInfo();
    wxSizer* createPrintButton();
    void     createProgressLine();
    wxSizer* createIconButtons();
    wxSizer* createVersionInfoSizer();
    wxSizer* createDetailedInfo();
    wxPanel* createSeperator();

    void updateProgressLine();
    void updateTimer();
    void updatePrintButton();
    void updateStatusText();
    void updateAvatar();
    void updateIconButtons();
    void updatePrintInfo();

    void applyDeviceName();
    void toggleDeviceNameWidgets();

    void confirm(std::function<void()> cb, const wxString& question = _L("Are you sure?"));
};
} // namespace Slic3r::GUI
