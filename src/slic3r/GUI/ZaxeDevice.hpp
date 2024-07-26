#pragma once

#include <wx/panel.h>
#include <wx/statbmp.h>

#include "Widgets/Label.hpp"
#include "Widgets/ProgressBar.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/Button.hpp"

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
    void enablePrintButton(bool enable);

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

    Button*           model_btn;
    Button*           model_btn_expanded;
    Label*            device_name;
    wxTextCtrl*       device_name_ctrl;
    Button*           expand_btn;
    wxBitmap          default_avatar;
    wxStaticBitmap*   avatar;
    RoundedRectangle* avatar_rect;
    Label*            status_title;
    Label*            status_desc;
    Button*           status_desc_icon;
    Button*           print_btn;
    ProgressBar*      progress_bar;
    Label*            progress_label;
    wxPanel*          progress_line;
    Button*           pause_btn;
    Button*           resume_btn;
    Button*           stop_btn;
    Button*           preheat_btn;
    Button*           say_hi_btn;
    Button*           unload_btn;
    Button*           toggle_leds_btn;
    Label*            material_val;
    Label*            nozzle_val;
    Label*            nozzle_temp_val;
    Label*            plate_temp_val;
    Label*            printing_file;
    Label*            printing_file_val;
    Label*            printing_time;
    Label*            printing_time_val;
    wxSizer*          detailed_info_sizer;
    Label*            version;

    bool device_name_ctrl_visible{false};
    bool is_expanded{false};
    bool is_file_name_visible{false};

    ZaxeDeviceCapabilities capabilities;
    std::optional<Semver>  upstream_version;
    bool                   update_available{false};

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