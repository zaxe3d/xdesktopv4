#include "ZaxeDevice.hpp"

#include "GUI_App.hpp"
#include "libslic3r/Utils.hpp"
#include "MsgDialog.hpp"

namespace Slic3r::GUI {
const wxString gray200{"#EAECF0"};
const wxString gray500{"#667085"};
const wxString gray700{"#344054"};
const wxString blue500{"#009ADE"};
const wxString grayText{"#98A2B3"};

const wxString warning_color{"#FFA500"};
const wxString danger_color{"#FF0000"};
const wxString success_color{"#009BDF"};
const wxString uploading_color{"#00FF00"};

ZaxeDevice::ZaxeDevice(NetworkMachine* _nm, wxWindow* parent, wxPoint pos, wxSize size)
    : wxPanel(parent, wxID_ANY, pos, size), nm(_nm), timer(new wxTimer())
{
    SetBackgroundColour(*wxWHITE);
    wxGetApp().UpdateDarkUI(this);

    auto header_sizer = createHeader();
    createAvatar();
    auto state_info_sizer = createStateInfo();
    auto print_btn_sizer  = createPrintButton();
    createProgressLine();
    auto seperator = createSeperator();

    auto s1 = new wxBoxSizer(wxVERTICAL);
    s1->Add(state_info_sizer, 0, wxEXPAND | wxALL, FromDIP(1));
    s1->Add(progress_line, 0, wxEXPAND | wxALL, FromDIP(3));
    s1->Add(print_btn_sizer, 1, wxALL, FromDIP(1));

    auto body_sizer = new wxBoxSizer(wxHORIZONTAL);
    body_sizer->Add(avatar_rect, 0, wxALL, FromDIP(1));
    body_sizer->Add(s1, 1, wxEXPAND | wxALIGN_CENTER | wxALL, FromDIP(1));

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(header_sizer, 0, wxEXPAND | wxALL, FromDIP(3));
    sizer->Add(body_sizer, 1, wxEXPAND | wxALL, FromDIP(3));
    sizer->Add(seperator, 0, wxRIGHT | wxEXPAND, 20);

    SetSizer(sizer);

    updateStates();
    Layout();

    timer->Bind(wxEVT_TIMER, [&](wxTimerEvent& evt) { onTimer(evt); });
}

ZaxeDevice::~ZaxeDevice()
{
    if (timer->IsRunning()) {
        timer->Stop();
    }
}

void ZaxeDevice::onTimer(wxTimerEvent& event)
{
    return;
    /*
    m_txtFileTime->SetLabel(_L("Elapsed / Estimated time: ") +
        get_time_hms(
            wxDateTime::Now().GetTicks() -
            this->nm->attr->startTime -
            // make it look paused by incrementing and subtracting from the counter.
            (nm->states->paused ? m_pausedSeconds++ : m_pausedSeconds))
        + " / " + nm->attr->estimatedTime);
    */
}

wxSizer* ZaxeDevice::createHeader()
{
    auto model_rect = new RoundedRectangle(this, gray500, wxDefaultPosition, wxSize(FromDIP(36), FromDIP(24)), FromDIP(7));
    wxGetApp().UpdateDarkUI(model_rect);

    string model_str = to_upper_copy(nm->attr->deviceModel);
    boost::replace_all(model_str, "PLUS", "+");
    auto model_label = new Label(model_rect, wxString(model_str.c_str(), wxConvUTF8), wxALIGN_LEFT);
    model_label->SetFont(::Label::Body_14);
    model_label->SetForegroundColour(*wxWHITE);
    model_label->SetBackgroundColour(gray500);
    wxGetApp().UpdateDarkUI(model_label);

    auto model_sizer = new wxBoxSizer(wxVERTICAL);
    model_sizer->Add(model_label, 0, wxALL | wxALIGN_CENTER, FromDIP(3));

    model_rect->SetSizer(model_sizer);
    model_rect->Layout();

    device_name = new Label(this, wxString(nm->name.c_str(), wxConvUTF8), wxALIGN_CENTER_VERTICAL);
    device_name->SetFont(::Label::Head_18);
    device_name->SetForegroundColour(grayText);
    wxGetApp().UpdateDarkUI(device_name);

    device_name_ctrl = new wxTextCtrl(this, wxID_ANY, wxString(nm->name.c_str(), wxConvUTF8), wxDefaultPosition, wxDefaultSize,
                                      wxTE_LEFT | wxSIMPLE_BORDER | wxTE_PROCESS_ENTER);
    device_name_ctrl->SetMaxLength(15);
    device_name_ctrl->Hide();
    wxGetApp().UpdateDarkUI(device_name_ctrl);
    device_name_ctrl_visible = false;

    auto more_btn = new Button(this, "", "zaxe_more", wxBORDER_NONE, FromDIP(12));
    wxGetApp().UpdateDarkUI(more_btn);

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(model_rect, 0, wxALIGN_LEFT | wxALL, FromDIP(1));
    sizer->Add(device_name, 0, wxALIGN_LEFT | wxALL, FromDIP(1));
    sizer->Add(device_name_ctrl, 10, wxALIGN_LEFT | wxALL, FromDIP(1));
    sizer->AddStretchSpacer(1);
    sizer->Add(more_btn, 0, wxALIGN_RIGHT | wxALL, FromDIP(3));

    device_name->Bind(wxEVT_LEFT_UP, [&](auto& evt) {
        toggleDeviceNameWidgets();
        device_name_ctrl->SetFocus();
        device_name_ctrl->SetInsertionPointEnd();
    });

    device_name_ctrl->Bind(wxEVT_CHAR_HOOK, [&](auto& evt) {
        if (evt.GetKeyCode() == WXK_ESCAPE) {
            setName(nm->name);
            toggleDeviceNameWidgets();
        }
        evt.Skip();
    });

    device_name_ctrl->Bind(wxEVT_TEXT_ENTER, [&](auto& evt) {
        applyDeviceName();
        toggleDeviceNameWidgets();
        evt.Skip();
    });

    device_name_ctrl->Bind(wxEVT_KILL_FOCUS, [&](auto& evt) {
        applyDeviceName();
        toggleDeviceNameWidgets();
        evt.Skip();
    });

    return sizer;
}

void ZaxeDevice::createAvatar()
{
    avatar_rect = new RoundedRectangle(this, gray200, wxDefaultPosition, wxSize(FromDIP(75), FromDIP(75)), FromDIP(8), 1);
    wxGetApp().UpdateDarkUI(avatar_rect);
    default_avatar = create_scaled_bitmap("zaxe_printer", avatar_rect, 48);
    avatar         = new wxStaticBitmap(avatar_rect, wxID_ANY, default_avatar, wxDefaultPosition, wxDefaultSize);

    auto avatar_sizer = new wxBoxSizer(wxVERTICAL);
    avatar_sizer->Add(avatar, 1, wxEXPAND | wxALIGN_CENTER, 0);

    avatar_rect->SetSizer(avatar_sizer);
    avatar_rect->Layout();
}

wxSizer* ZaxeDevice::createStateInfo()
{
    status_title = new Label(this, "");
    status_title->SetFont(::Label::Head_12);
    status_title->SetForegroundColour(gray700);
    wxGetApp().UpdateDarkUI(status_title);
    status_title->Hide();

    status_desc = new Label(this, "");
    status_desc->SetFont(::Label::Body_12);
    status_desc->SetForegroundColour(gray700);
    wxGetApp().UpdateDarkUI(status_desc);
    status_desc->Hide();

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(status_title, 0, wxEXPAND | wxALL, FromDIP(1));
    sizer->Add(status_desc, 0, wxEXPAND | wxALL, FromDIP(1));
    return sizer;
}

wxSizer* ZaxeDevice::createPrintButton()
{
    print_btn = new Button(this, _L("Print"));
    print_btn->SetMinSize(wxSize(FromDIP(64), FromDIP(36)));
    print_btn->SetBackgroundColor(*wxWHITE);
    print_btn->SetBorderColor(blue500);
    print_btn->SetTextColor(blue500);
    wxGetApp().UpdateDarkUI(print_btn);

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(print_btn, 0, wxALIGN_CENTER | wxALL, FromDIP(1));

    print_btn->Bind(wxEVT_BUTTON, [this](auto& evt) { onPrintDenied(); });

    return sizer;
}

void ZaxeDevice::createProgressLine()
{
    progress_line = new wxPanel(this);
    progress_line->SetBackgroundColour(*wxWHITE);
    wxGetApp().UpdateDarkUI(progress_line);

    progress_bar = new ProgressBar(progress_line);
    progress_bar->ShowNumber(true);
    progress_bar->SetProgressForedColour(gray200);
    progress_bar->SetProgressBackgroundColour(blue500);
    progress_bar->SetTextColour(*wxBLACK);
    wxGetApp().UpdateDarkUI(progress_bar);

    auto pause_btn = new Button(progress_line, "", "zaxe_pause", wxBORDER_NONE, FromDIP(20));
    pause_btn->SetPaddingSize(wxSize(2, 2));
    wxGetApp().UpdateDarkUI(pause_btn);

    auto stop_btn = new Button(progress_line, "", "zaxe_stop", wxBORDER_NONE, FromDIP(20));
    stop_btn->SetPaddingSize(wxSize(2, 2));
    wxGetApp().UpdateDarkUI(stop_btn);

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(progress_bar, 1, wxALIGN_CENTER | wxALL, FromDIP(1));
    sizer->Add(pause_btn, 0, wxALL, FromDIP(1));
    sizer->Add(stop_btn, 0, wxALL, FromDIP(1));

    progress_line->SetSizer(sizer);
    progress_line->Layout();

    nm->setUploadProgressCallback([&](int progress) {
        if (progress <= 0 || progress >= 100) {
            updateStates();
        };
        updateProgressValue();
    });
}

wxStaticLine* ZaxeDevice::createSeperator()
{
    auto seperator = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(4)), wxLI_HORIZONTAL);
    seperator->SetBackgroundColour(gray200);
    wxGetApp().UpdateDarkUI(seperator);
    return seperator;
}

void ZaxeDevice::updateStates()
{
    updateProgressLine();
    updateTimer();
    updatePrintButton();
    updateStatusText();
    updateAvatar();

    Layout();
    Refresh();
}

void ZaxeDevice::updateProgressLine()
{
    bool show = nm->isBusy() && !nm->states->hasError;
    progress_line->Show(show);
    updateProgressValue();

    if (nm->states->heating) {
        progress_bar->SetProgressBackgroundColour(danger_color);
    } else if (nm->states->uploading) {
        progress_bar->SetProgressBackgroundColour(uploading_color);
    } else if (nm->states->calibrating) {
        progress_bar->SetProgressBackgroundColour(warning_color);
    } else {
        progress_bar->SetProgressBackgroundColour(blue500);
    }
}

void ZaxeDevice::updateTimer()
{
    if (!nm->states->bedOccupied && !nm->states->heating && !nm->states->paused && nm->states->printing) {
        if (!timer->IsRunning()) {
            timer->Start(1000);
        }
    }
    if (!nm->states->printing && timer->IsRunning()) {
        timer->Stop();
    }
}

void ZaxeDevice::updateProgressValue()
{
    progress_bar->SetProgress(nm->progress);
    Layout();
    Refresh();
}

void ZaxeDevice::updatePrintButton()
{
    bool show = !nm->isBusy() && !nm->states->bedOccupied && !nm->states->hasError;
    print_btn->Show(show);
}

void ZaxeDevice::updateStatusText()
{
    wxString title = "";
    wxString desc  = "";

    if (nm->states->bedOccupied) {
        title = _L("Bed is occupied");
    } else if (nm->states->heating) {
        title = _L("Heating");
    } else if (nm->states->paused) {
        title = _L("Paused");
    } else if (nm->states->printing) {
        title = _L("Printing");
    } else if (nm->states->uploading) {
        title = _L("Uploading");
    } else if (nm->states->calibrating) {
        title = _L("Calibrating");
    } else if (!nm->isBusy()) {
        title = _L("Ready to use");
    }

    if (nm->states->hasError) {
        desc = _L("Device is in error state!");
    } else if (!nm->isBusy() && nm->states->bedOccupied) {
        desc = _L("Please take your print!");
    } else if (nm->states->printing) {
        desc = _L("Processing");
    }

    status_title->SetLabel(title);
    status_title->Show(!title.empty());

    status_desc->SetLabel(desc);
    status_desc->Show(!desc.empty());
}

void ZaxeDevice::updateAvatar()
{
    if (nm->attr->hasSnapshot) {
        if (nm->states->heating || nm->states->printing || nm->states->calibrating || nm->states->bedOccupied) {
            nm->downloadAvatar();
        } else {
            avatar->SetBitmap(default_avatar);
        }
    }
}

void ZaxeDevice::onAvatarReady()
{
    if (nm) {
        avatar->SetBitmap(nm->getAvatar());
    }
}

void ZaxeDevice::enablePrintButton(bool enable) { print_btn->Enable(true); }

void ZaxeDevice::onPrintDenied()
{
    nm->states->updating = false;
    RichMessageDialog dialog(GetParent(), _L("Failed to start printing"), _L("XDesktop: Unknown error"), wxICON_ERROR);
    dialog.ShowModal();
}

bool ZaxeDevice::isBusy() { return nm->isBusy(); }

void ZaxeDevice::setName(const string& name)
{
    device_name->SetLabel(wxString(name.c_str(), wxConvUTF8));
    device_name_ctrl->SetValue(wxString(name.c_str(), wxConvUTF8));
}

wxString ZaxeDevice::getName() const { return device_name->GetLabel(); }

void ZaxeDevice::applyDeviceName()
{
    auto val = device_name_ctrl->GetValue().Strip(wxString::both);
    if (val.empty()) {
        setName(nm->name);
    } else if (nm->name != val.ToStdString()) {
        nm->changeName(val.char_str());
    }
}

void ZaxeDevice::toggleDeviceNameWidgets()
{
    if (device_name_ctrl_visible) {
        device_name_ctrl->Hide();
        device_name->Show();
    } else {
        device_name->Hide();
        device_name_ctrl->Show();
    }

    device_name_ctrl_visible = !device_name_ctrl_visible;

    Layout();
    Refresh();
}

} // namespace Slic3r::GUI
