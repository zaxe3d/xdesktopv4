#ifndef slic3r_Device_hpp_
#define slic3r_Device_hpp_

#include <wx/graphics.h>
#include <wx/panel.h>
#include <wx/statline.h>
#include <wx/gauge.h>
#include <boost/log/trivial.hpp>
#include <mutex>

#include "../Utils/NetworkMachine.hpp"
#include "I18N.hpp"

namespace Slic3r {
namespace GUI {

#define DEVICE_HEIGHT 80
#define DEVICE_FILENAME_MAX_NUM_CHARS 40
#define DEVICE_COLOR_ZAXE_BLUE wxColor(0, 155, 223)
#define DEVICE_COLOR_ORANGE wxColor(255, 165, 0)
#define DEVICE_COLOR_DANGER wxColor(255, 0, 0)
#define DEVICE_COLOR_SUCCESS wxColor(0, 155, 223)
#define DEVICE_COLOR_UPLOADING *wxGREEN

class RoundedPanel: public wxPanel
{
public:
    RoundedPanel(wxPanel *parent, int id, wxString label, wxSize size, wxColour bgColour, wxColour fgColour);

    void OnPaint(wxPaintEvent& event);
    void SetAvatar(wxBitmap& bmp);
    void Clear();
    void SetText(wxString& text);
private:
    wxRect ShrinkToSize(const wxRect &container, const int margin, const int thickness);

    wxColour m_bgColour;
    wxColour m_fgColour;
    wxString m_label;
    wxBitmap m_avatar;
};

class CustomProgressBar: public wxPanel
{
public:
    CustomProgressBar(wxPanel *parent, int id, wxSize size);

    void OnPaint(wxPaintEvent& event);
    void SetValue(uint8_t value) { m_progress = value; Refresh(); }
    void SetColour(wxColour c) { m_fgColour = c; Refresh(); }
private:
    wxColour m_fgColour;
    uint8_t m_progress;
};

class Device : public wxPanel {
public:
    Device(NetworkMachine* nm, wxWindow* parent);
    ~Device();
    wxString name;
    NetworkMachine* nm; // network machine.

    void setName(const string &name);
    wxString getName() const;
    void setMaterialLabel(const string &material_label);
    void setFilamentPresent(const bool present);
    void setNozzle(const string &nozzle);
    void setPin(const bool hasPin);
    void setFileStart();
    void updateStates();
    void updateProgress();
    void updateStatus();
    void avatarReady();

    void enablePrintNowButton(bool enable);

    void onTimer(wxTimerEvent& event);
    void onModeChanged();
    
    bool print();

private:
    wxTimer* m_timer; // elapsed timer.
    void confirm(std::function<void()> cb, const wxString& question=_L("Are you sure?"));
    wxSizer* m_mainSizer; // vertical sizer (device sizer - horizontal line (seperator).
    wxSizer* m_deviceSizer; // horizontal sizer (avatar | right pane)).
    wxSizer* m_expansionSizer; // vertical sizer (filament | printing time etc.)
    wxSizer* m_filamentSizer; // horizontal sizer (filament | unload filament button).
    wxSizer* m_rightSizer; // vertical right pane. (name - status - progress bar).

    CustomProgressBar* m_progressBar; // progress bar.
    wxBitmapButton* m_btnSayHi; // hi button.
    wxBitmapButton* m_btnPreheat; // preheat button.
    wxBitmapButton* m_btnPause; // pause button.
    wxBitmapButton* m_btnResume; // resume button.
    wxBitmapButton* m_btnCancel; // cancel button.
    wxBitmapButton* m_btnExpandCollapse; // expand/collapse button.
    wxButton* m_btnUnload; // unload filament button.
    wxButton* m_btnPrintNow; // print now button.
    wxStaticText* m_txtStatus; // status text.
    wxStaticText* m_txtProgress; // progress text.
    wxStaticText* m_txtDeviceName; // device name text.
    wxTextCtrl* m_txtCtrlDeviceName; // device name text control.
    wxStaticText* m_txtDeviceIP; // device IP text.
    wxStaticText* m_txtDeviceMaterial; // device material text.
    wxStaticText* m_txtDeviceNozzleDiameter; // device nozzle text.
    wxStaticText* m_txtFileName; // file name text.
    wxStaticText* m_txtFileTime; // file elapsed time / estimated time text.
    wxStaticText* m_txtBedOccupiedMessage; // bed occuppied message under status text.
    wxStaticText* m_txtErrorMessage; // bed occuppied message under status text.
    wxStaticText* m_txtFWVersion; // firmware version at the very bottom.

    RoundedPanel* m_avatar;

    wxBitmap* m_bitPreheatActive;
    wxBitmap* m_bitPreheatDeactive;
    wxBitmap* m_bitExpanded;
    wxBitmap* m_bitCollapsed;

    bool m_isExpanded;

    int m_pausedSeconds;

    std::mutex m_deviceNameMtx;
    bool m_deviceNameTxtCtrlShown;
    void applyDeviceName();
    void toggleDeviceNameWidgets();

    int getDeviceExtraHeight() const;
};
} // namespace GUI
} // namespace Slic3r
#endif
