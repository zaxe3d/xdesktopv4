#pragma once

#include <wx/panel.h>
#include <wx/statbmp.h>
#include <wx/statline.h>

#include "Widgets/Label.hpp"
#include "Widgets/ProgressBar.hpp"
#include "Widgets/RoundedRectangle.hpp"
#include "Widgets/Button.hpp"

#include "../Utils/NetworkMachine.hpp"
#include "I18N.hpp"

namespace Slic3r::GUI {
class ZaxeDevice : public wxPanel
{
public:
    ZaxeDevice(NetworkMachine* _nm, wxWindow* parent, wxPoint pos = wxDefaultPosition, wxSize size = wxDefaultSize);
    ~ZaxeDevice();

    void updateStates();
    void updateProgressValue();
    void onAvatarReady();
    void enablePrintButton(bool enable);
    void onPrintDenied();

    bool     isBusy();
    void     setName(const string& name);
    wxString getName() const;

private:
    NetworkMachine* nm;
    wxTimer*        timer;

    Label*            device_name;
    wxTextCtrl*       device_name_ctrl;
    wxBitmap          default_avatar;
    wxStaticBitmap*   avatar;
    RoundedRectangle* avatar_rect;
    Label*            status_title;
    Label*            status_desc;
    Button*           print_btn;
    ProgressBar*      progress_bar;
    Label*            progress_label;
    wxPanel*          progress_line;

    bool device_name_ctrl_visible{false};

    void          onTimer(wxTimerEvent& event);
    wxSizer*      createHeader();
    void          createAvatar();
    wxSizer*      createStateInfo();
    wxSizer*      createPrintButton();
    void          createProgressLine();
    wxStaticLine* createSeperator();

    void updateProgressLine();
    void updateTimer();
    void updatePrintButton();
    void updateStatusText();
    void updateAvatar();

    void applyDeviceName();
    void toggleDeviceNameWidgets();
};
} // namespace Slic3r::GUI
