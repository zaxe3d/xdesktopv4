#include "ZaxeDevice.hpp"

#include "libslic3r/Utils.hpp"
#include "MsgDialog.hpp"
#include "Plater.hpp"
#include "GUI_App.hpp"

namespace Slic3r::GUI {
const wxString gray100{"#F2F4F7"};
const wxString gray200{"#EAECF0"};
const wxString gray300{"#D0D5DD"};
const wxString gray400{"#98A2B3"};
const wxString gray500{"#667085"};
const wxString gray700{"#344054"};
const wxString blue500{"#009ADE"};

const wxString progress_calib_color{"#FFA500"};
const wxString progress_danger_color{"#F25A46"};
const wxString progress_success_color{"#009BDF"};
const wxString progress_uploading_color{"#00FF00"};

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
    auto icon_btns_sizer = createIconButtons();
    detailed_info_sizer  = createDetailedInfo();
    auto seperator       = createSeperator();

    auto s1 = new wxBoxSizer(wxHORIZONTAL);
    s1->Add(progress_line, 19, wxALIGN_CENTER | wxALL, FromDIP(3));
    s1->Add(print_btn_sizer, 0, wxALIGN_CENTER | wxALL, FromDIP(1));
    s1->AddStretchSpacer(1);
    s1->Add(icon_btns_sizer, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(1));

    auto s2 = new wxBoxSizer(wxVERTICAL);
    s2->Add(state_info_sizer, 0, wxEXPAND | wxALL, FromDIP(1));
    s2->Add(s1, 0, wxEXPAND | wxALL, FromDIP(1));

    auto body_sizer = new wxBoxSizer(wxHORIZONTAL);
    body_sizer->Add(avatar_rect, 0, wxALL, FromDIP(1));
    body_sizer->Add(s2, 1, wxALIGN_CENTER | wxALL, FromDIP(1));

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(header_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(3));
    sizer->Add(body_sizer, 1, wxEXPAND | wxALL, FromDIP(3));
    sizer->Add(detailed_info_sizer, 0, wxALIGN_CENTER | wxALL, FromDIP(10));
    sizer->Add(seperator, 0, wxEXPAND | wxTOP, FromDIP(5));

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
    printing_time_val->SetLabel(get_time_hms(wxDateTime::Now().GetTicks() - nm->attr->startTime) + " / " + nm->attr->estimatedTime);
}

wxSizer* ZaxeDevice::createHeader()
{
    string model_str = to_upper_copy(nm->attr->deviceModel);
    boost::replace_all(model_str, "PLUS", "+");

    model_btn = new Button(this, wxString(model_str.c_str(), wxConvUTF8), "", wxBORDER_NONE, FromDIP(24));
    model_btn->SetPaddingSize(wxSize(4, 3));
    model_btn->SetTextColor(*wxWHITE);
    model_btn->SetBackgroundColor(gray500);
    wxGetApp().UpdateDarkUI(model_btn);
    model_btn->Show(!is_expanded);

    model_btn_expanded = new Button(this, wxString(model_str.c_str(), wxConvUTF8), "", wxBORDER_NONE, FromDIP(24));
    model_btn_expanded->SetPaddingSize(wxSize(4, 3));
    model_btn_expanded->SetTextColor(*wxWHITE);
    model_btn_expanded->SetBackgroundColor(blue500);
    wxGetApp().UpdateDarkUI(model_btn_expanded);
    model_btn_expanded->Show(is_expanded);

    device_name = new Label(this, wxString(nm->name.c_str(), wxConvUTF8), wxALIGN_CENTER_VERTICAL);
    device_name->SetFont(::Label::Body_16);
    device_name->SetForegroundColour(gray400);
    wxGetApp().UpdateDarkUI(device_name);

    device_name_ctrl = new wxTextCtrl(this, wxID_ANY, wxString(nm->name.c_str(), wxConvUTF8), wxDefaultPosition, wxDefaultSize,
                                      wxTE_LEFT | wxSIMPLE_BORDER | wxTE_PROCESS_ENTER);
    device_name_ctrl->SetMaxLength(15);
    device_name_ctrl->Hide();
    wxGetApp().UpdateDarkUI(device_name_ctrl);
    device_name_ctrl_visible = false;

    expand_btn = new Button(this, "", "zaxe_arrow_down", wxBORDER_NONE, FromDIP(24));
    expand_btn->SetPaddingSize(wxSize(3, 3));
    wxGetApp().UpdateDarkUI(expand_btn);
    is_expanded = false;

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(model_btn, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(1));
    sizer->Add(model_btn_expanded, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(1));
    sizer->Add(device_name, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(1));
    sizer->Add(device_name_ctrl, 10, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(1));
    sizer->AddStretchSpacer(1);
    sizer->Add(expand_btn, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(3));

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

    expand_btn->Bind(wxEVT_BUTTON, [&](auto& e) {
        is_expanded = !is_expanded;
        expand_btn->SetIcon(is_expanded ? "zaxe_arrow_up_active" : "zaxe_arrow_down");
        model_btn->Show(!is_expanded);
        model_btn_expanded->Show(is_expanded);
        device_name->SetForegroundColour(is_expanded ? blue500 : gray400);
        detailed_info_sizer->Show(is_expanded);
        updatePrintInfo();
        Layout();
        GetParent()->Layout();
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

    if (is_there(nm->attr->deviceModel, {"z2", "z3"})) {
        avatar_rect->Bind(wxEVT_LEFT_DCLICK, [&](auto& e) {
            BOOST_LOG_TRIVIAL(info) << "Clicked on avatar trying to open stream on: " << nm->name;
            if (nm->attr->firmwareVersion.GetMinor() >= 4 ||
                (nm->attr->firmwareVersion.GetMinor() >= 3 && nm->attr->firmwareVersion.GetMicro() >= 80)) {
                wxFileName ffplay(wxStandardPaths::Get().GetExecutablePath());
                wxString   curExecPath(ffplay.GetPath());

                wxExecute(
#ifdef _WIN32
                    "cmd.exe /c ffplay tcp://" + nm->ip + ":5002 -window_title \"Zaxe " + to_upper_copy(nm->attr->deviceModel) + ": " +
                        nm->name + "\" -x 720",
                    wxEXEC_ASYNC | wxEXEC_HIDE_CONSOLE
#else
                    curExecPath + "/ffplay tcp://" + nm->ip + ":5002 -window_title \"Zaxe " + to_upper_copy(nm->attr->deviceModel) + ": " + nm->name + "\" -x 720",
                    wxEXEC_ASYNC
#endif
                );
            } else {
                wxMessageBox("Need device firmware version at least v3.3.80 to comply.", "Need firmware update for this feautre.",
                             wxICON_INFORMATION);
            }
        });
    }
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

    status_desc_icon = new Button(this, "", "zaxe_red_warning", wxBORDER_NONE, FromDIP(24));
    status_desc_icon->SetPaddingSize(wxSize(0, 0));
    wxGetApp().UpdateDarkUI(status_desc_icon);
    status_desc_icon->Hide();

    auto desc_sizer = new wxBoxSizer(wxHORIZONTAL);
    desc_sizer->Add(status_desc_icon, 0, wxALIGN_CENTER);
    desc_sizer->Add(status_desc, 0, wxALIGN_CENTER);

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(status_title, 0, wxEXPAND | wxALL, FromDIP(1));
    sizer->Add(desc_sizer, 0, wxEXPAND | wxALL, FromDIP(1));
    return sizer;
}

wxSizer* ZaxeDevice::createPrintButton()
{
    print_btn = new Button(this, _L("Print now"));
    print_btn->SetPaddingSize(wxSize(8, 5));
    print_btn->SetBackgroundColor(*wxWHITE);
    auto color = StateColor(std::pair<wxColour, int>(gray300, StateColor::Disabled), std::pair<wxColour, int>(blue500, StateColor::Normal));
    print_btn->SetBorderColor(color);
    print_btn->SetTextColor(color);
    wxGetApp().UpdateDarkUI(print_btn);

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(print_btn, 0, wxALIGN_CENTER | wxALL, FromDIP(1));

    print_btn->Bind(wxEVT_BUTTON, [this](auto& evt) {
        print_btn->Enable(false);
        BOOST_LOG_TRIVIAL(info) << "Print now pressed on " << nm->name;
        print();
        print_btn->Enable(true);
    });

    return sizer;
}

void ZaxeDevice::createProgressLine()
{
    progress_line = new wxPanel(this);
    progress_line->SetBackgroundColour(*wxWHITE);
    wxGetApp().UpdateDarkUI(progress_line);

    progress_bar = new ProgressBar(progress_line);
    progress_bar->ShowNumber(false);
    progress_bar->SetProgressForedColour(gray200);
    progress_bar->SetProgressBackgroundColour(blue500);
    wxGetApp().UpdateDarkUI(progress_bar);

    progress_label = new Label(progress_line, "", wxALIGN_CENTER_VERTICAL);
    progress_label->SetFont(Label::Head_14);

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(progress_bar, 1, wxALIGN_CENTER | wxALL, FromDIP(1));
    sizer->Add(progress_label, 0, wxALIGN_CENTER | wxALL, FromDIP(1));

    progress_line->SetSizer(sizer);
    progress_line->Layout();

    nm->setUploadProgressCallback([&](int progress) {
        if (progress <= 0 || progress >= 100) {
            updateStates();
        };
        updateProgressValue();
    });
}

wxSizer* ZaxeDevice::createIconButtons()
{
    auto create_icon_btn = [&](const auto& icon_name, const wxString& tool_tip, auto callback) {
        auto       btn = new Button(this, "", icon_name, wxBORDER_NONE, FromDIP(24));
        StateColor btn_bg(std::pair<wxColour, int>(gray300, StateColor::Pressed), std::pair<wxColour, int>(gray200, StateColor::Hovered),
                          std::pair<wxColour, int>(gray100, StateColor::Normal));
        btn->SetBackgroundColor(btn_bg);
        btn->SetPaddingSize(wxSize(2, 2));
        btn->SetToolTip(tool_tip);
        wxGetApp().UpdateDarkUI(btn);
        btn->Bind(wxEVT_BUTTON, callback);
        return btn;
    };

    pause_btn   = create_icon_btn("zaxe_pause", _L("Pause"), [&](const auto& evt) { confirm([&] { nm->pause(); }); });
    resume_btn  = create_icon_btn("zaxe_resume", _L("Resume"), [&](const auto& evt) { confirm([&] { nm->resume(); }); });
    stop_btn    = create_icon_btn("zaxe_stop", _L("Stop"), [&](const auto& evt) { confirm([&] { nm->cancel(); }); });
    preheat_btn = create_icon_btn("zaxe_preheat", _L("Preheat"), [&](const auto& evt) { confirm([&] { nm->togglePreheat(); }); });
    say_hi_btn  = create_icon_btn("zaxe_hello", _L("Say Hi!"), [&](const auto& evt) { confirm([&] { nm->sayHi(); }); });
    unload_btn  = create_icon_btn("zaxe_unload", _L("Unload filament"), [&](const auto& evt) { confirm([&] { nm->unloadFilament(); }); });

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(pause_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(resume_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(stop_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(preheat_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(say_hi_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(unload_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Layout();
    return sizer;
}

wxSizer* ZaxeDevice::createDetailedInfo()
{
    auto create_label = [&](const auto& font, const wxString& txt) {
        auto l = new Label(this, txt, wxALIGN_LEFT);
        l->SetFont(font);
        wxGetApp().UpdateDarkUI(l);
        return l;
    };

    wxFont bold_font   = ::Label::Head_12;
    wxFont normal_font = ::Label::Body_12;

    printing_file     = create_label(bold_font, _L("File"));
    printing_file_val = create_label(normal_font, wxString(nm->attr->printingFile.c_str(), wxConvUTF8));
    printing_file_val->Wrap(FromDIP(200));

    printing_time     = create_label(bold_font, _L("Elapsed / Estimated time"));
    printing_time_val = create_label(normal_font, "");

    auto material = create_label(bold_font, _L("Material"));
    material_val  = create_label(normal_font, "");

    auto nozzle = create_label(bold_font, _L("Nozzle"));
    nozzle_val  = create_label(normal_font, nm->attr->isLite ? "-" : nm->attr->nozzle + "mm");

    auto nozzle_temp = create_label(bold_font, _L("Nozzle Temp."));
    nozzle_temp_val  = create_label(normal_font, "");

    auto plate_temp = create_label(bold_font, _L("Plate Temp."));
    plate_temp_val  = create_label(normal_font, "");

    auto ip_addr     = create_label(bold_font, _L("IP Address"));
    auto ip_addr_val = create_label(normal_font, nm->ip);

    auto sizer = new wxFlexGridSizer(6, 2, FromDIP(5), FromDIP(15));
    sizer->Add(printing_file, 0, wxEXPAND);
    sizer->Add(printing_file_val, 0, wxEXPAND);
    sizer->Add(printing_time, 0, wxEXPAND);
    sizer->Add(printing_time_val, 0, wxEXPAND);
    sizer->Add(material, 0, wxEXPAND);
    sizer->Add(material_val, 0, wxEXPAND);
    sizer->Add(nozzle, 0, wxEXPAND);
    sizer->Add(nozzle_val, 0, wxEXPAND);
    sizer->Add(nozzle_temp, 0, wxEXPAND);
    sizer->Add(nozzle_temp_val, 0, wxEXPAND);
    sizer->Add(plate_temp, 0, wxEXPAND);
    sizer->Add(plate_temp_val, 0, wxEXPAND);
    sizer->Add(ip_addr, 0, wxEXPAND);
    sizer->Add(ip_addr_val, 0, wxEXPAND);

    sizer->Show(false);

    return sizer;
}

wxPanel* ZaxeDevice::createSeperator()
{
    auto seperator = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)));
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
    updateIconButtons();
    setFilamentPresent(nm->states->filamentPresent);

    Layout();
    Refresh();
}

void ZaxeDevice::updateProgressLine()
{
    bool show = nm->isBusy() && !nm->states->hasError;
    progress_line->Show(show);
    updateProgressValue();

    if (nm->states->heating) {
        progress_bar->SetProgressBackgroundColour(progress_danger_color);
    } else if (nm->states->uploading) {
        progress_bar->SetProgressBackgroundColour(progress_uploading_color);
    } else if (nm->states->calibrating) {
        progress_bar->SetProgressBackgroundColour(progress_calib_color);
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
    progress_label->SetLabel(wxString::Format("%d%%", nm->progress));
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
    wxString title      = "";
    wxString desc       = "";
    wxString desc_color = gray700;
    wxString desc_icon  = "";

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
        desc       = _L("Device is in error state!");
        desc_color = "#E22005";
        desc_icon  = "zaxe_red_warning";
    } else if (!nm->isBusy() && nm->states->bedOccupied) {
        desc       = _L("Please take your print!");
        desc_color = progress_success_color;
    } else if (nm->states->printing) {
        desc = _L("Processing");
    }

    status_title->SetLabel(title);
    status_title->Show(!title.empty());

    if (!desc_icon.empty()) {
        status_desc_icon->SetIcon(desc_icon);
    }
    status_desc_icon->Show(!desc_icon.empty());

    status_desc->SetLabel(desc);
    status_desc->SetForegroundColour(desc_color);
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

void ZaxeDevice::updateIconButtons()
{
    bool family_z = is_there(nm->attr->deviceModel, {"z"});

    pause_btn->Show(nm->states->printing && !nm->states->paused && !nm->states->heating);
    resume_btn->Show(nm->states->printing && nm->states->paused && !nm->states->heating);
    stop_btn->Show(nm->isBusy() && (nm->states->printing || !nm->states->uploading));

    preheat_btn->Show(!nm->isBusy() && !nm->states->bedOccupied && !nm->states->hasError);
    preheat_btn->SetIcon(nm->states->preheat ? "zaxe_preheat_active" : "zaxe_preheat");

    say_hi_btn->Show(!nm->isBusy() && !nm->states->bedOccupied && !nm->states->hasError);

    unload_btn->Show(family_z && !nm->isBusy() && !nm->states->bedOccupied && !nm->states->hasError);
}

void ZaxeDevice::updatePrintInfo()
{
    printing_file->Show(is_expanded && nm->states->printing);
    printing_file_val->Show(is_expanded && nm->states->printing);

    printing_time->Show(is_expanded && nm->states->printing);
    printing_time_val->Show(is_expanded && nm->states->printing);
}

void ZaxeDevice::onTemperatureUpdate()
{
    nozzle_temp_val->SetLabel(wxString::Format("%.1f "
                                               "\xC2\xB0"
                                               "C",
                                               nm->attr->nozzle_temp));
    plate_temp_val->SetLabel(wxString::Format("%.1f "
                                              "\xC2\xB0"
                                              "C",
                                              nm->attr->bed_temp));
    Refresh();
}

void ZaxeDevice::onAvatarReady()
{
    if (nm) {
        avatar->SetBitmap(nm->getAvatar());
        avatar_rect->Layout();
    }
}

void ZaxeDevice::enablePrintButton(bool enable) { print_btn->Enable(enable); }

void ZaxeDevice::onPrintDenied()
{
    RichMessageDialog dialog(GetParent(), _L("Failed to start printing"), _L("XDesktop: Unknown error"), wxICON_ERROR);
    dialog.ShowModal();

    nm->states->updating = false;
    updateStates();
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

void ZaxeDevice::confirm(function<void()> cb, const wxString& question)
{
    RichMessageDialog dialog(GetParent(), question, _L("XDesktop: Confirmation"), wxICON_QUESTION | wxYES_NO);
    dialog.SetYesNoLabels(_L("Yes"), _L("No"));
    int res = dialog.ShowModal();
    if (res == wxID_YES)
        cb();
}

void ZaxeDevice::setMaterialLabel(const std::string& material_label)
{
    if (nm->attr->firmwareVersion.GetMajor() >= 3 && nm->attr->firmwareVersion.GetMinor() >= 5 && !nm->states->filamentPresent) {
        material_val->SetLabel(_L("Not installed"));
    } else {
        material_val->SetLabel(wxString(material_label.c_str(), wxConvUTF8));
    }
    Refresh();
}

void ZaxeDevice::setFilamentPresent(bool present) { setMaterialLabel(nm->attr->materialLabel); }

void ZaxeDevice::setPin(bool has_pin) { nm->attr->hasPin = has_pin; }

void ZaxeDevice::setNozzle(const std::string& nozzle)
{
    nozzle_val->SetLabel(nm->attr->nozzle + "mm");
    Refresh();
}

void ZaxeDevice::setFileStart() { printing_file_val->SetLabel(wxString(nm->attr->printingFile.c_str(), wxConvUTF8)); }

bool ZaxeDevice::has(const wxString& search_text)
{
    auto txt = search_text.Lower();

    return getName().Lower().Find(txt) != wxNOT_FOUND || wxString(nm->ip).Find(txt) != wxNOT_FOUND ||
           material_val->GetLabel().Lower().Find(txt) != wxNOT_FOUND || nozzle_val->GetLabel().Lower().Find(txt) != wxNOT_FOUND ||
           model_btn->GetLabel().Lower().Find(txt) != wxNOT_FOUND;
}

bool ZaxeDevice::print()
{
    bool               ready   = false;
    const ZaxeArchive& archive = wxGetApp().plater()->get_zaxe_archive();

    vector<string> sPV;
    split(sPV, GUI::wxGetApp().preset_bundle->printers.get_selected_preset().name, is_any_of("-"));
    string pN = sPV[0]; // ie: Zaxe Z3S - 0.6mm nozzle -> Zaxe Z3S
    string dM = to_upper_copy(nm->attr->deviceModel);
    auto   s  = pN.find(dM);
    boost::replace_all(dM, "PLUS", "+");
    trim(pN);

    std::string model_nozzle_attr = dM + " " + nm->attr->nozzle;
    std::string model_nozzle_arch = archive.get_info("model") + " " + archive.get_info("sub_model") + " " +
                                    archive.get_info("nozzle_diameter");

    if (s == std::string::npos || pN.length() != dM.length() + s) {
        wxMessageBox(_L("Device model does NOT match. Please reslice with "
                        "the correct model."),
                     _L("Wrong device model"), wxOK | wxICON_ERROR);
    } else if (!nm->attr->isLite && nm->attr->material != "custom" && nm->states->filamentPresent &&
               nm->attr->material.compare(archive.get_info("material")) != 0) {
        wxMessageBox(_L("Materials don't match with this device. Please "
                        "reslice with the correct material."),
                     _L("Wrong material type"), wxICON_ERROR);
    } else if (!nm->states->filamentPresent && nm->attr->firmwareVersion.GetMajor() >= 3 && nm->attr->firmwareVersion.GetMinor() >= 5) {
        confirm([&] { ready = true; }, _L("No filament sensed. Do you really want to continue printing?"));
    } else if (!nm->attr->isLite && !case_insensitive_compare(model_nozzle_attr, model_nozzle_arch)) {
        wxMessageBox(_L("Currently installed nozzle on device doesn't match with this "
                        "slice. Please reslice with the correct nozzle."),
                     _L("Wrong nozzle type"), wxICON_ERROR);
    } else {
        ready = true;
    }

    if (ready) {
        std::thread t([&]() {
            if (nm->attr->isLite) {
                nm->upload(wxGetApp().plater()->get_gcode_path().c_str(),
                           translate_chars(wxGetApp().plater()->get_filename().ToStdString()).c_str());
            } else
                nm->upload(wxGetApp().plater()->get_zaxe_code_path().c_str());
        });
        t.detach(); // crusial. otherwise blocks main thread.
        return true;
    }
    return false;
}

} // namespace Slic3r::GUI
