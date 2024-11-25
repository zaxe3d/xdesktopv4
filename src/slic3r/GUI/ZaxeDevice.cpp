#include "ZaxeDevice.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/ZaxeConfigHelper.hpp"

#include "MsgDialog.hpp"
#include "Plater.hpp"
#include "GUI_App.hpp"
#include "NotificationManager.hpp"
#include "ZaxeRegisterPrinterDialog.hpp"

#include <boost/algorithm/string.hpp>

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

ZaxeDevice::ZaxeDevice(wxWindow* parent, wxPoint pos, wxSize size) : wxPanel(parent, wxID_ANY, pos, size), timer(new wxTimer())
{
    SetBackgroundColour(*wxWHITE);
    wxGetApp().UpdateDarkUI(this);
}

void ZaxeDevice::switchNetworkMachine(std::shared_ptr<ZaxeNetworkMachine> _nm)
{
    if (nm) {
        nm->Unbind(EVT_MACHINE_UPDATE, &ZaxeDevice::onUpdateDevice, this);
        nm->Unbind(EVT_MACHINE_AVATAR_READY, &ZaxeDevice::onAvatarReady, this);

        if (isRemoteDevice()) {
            nm->Unbind(EVT_MACHINE_CLOSE, &ZaxeDevice::onClose, this);
        }
    }

    nm = _nm;
    if (!ready) {
        init();
    } else {
        updateStates();
    }

    nm->Bind(EVT_MACHINE_UPDATE, &ZaxeDevice::onUpdateDevice, this);
    nm->Bind(EVT_MACHINE_AVATAR_READY, &ZaxeDevice::onAvatarReady, this);
    if (isRemoteDevice()) {
        nm->Bind(EVT_MACHINE_CLOSE, &ZaxeDevice::onClose, this);
    }
}

void ZaxeDevice::init()
{
    auto header_sizer = createHeader();
    createAvatar();
    auto state_info_sizer = createStateInfo();
    auto print_btn_sizer  = createPrintButton();
    createProgressLine();
    auto icon_btns_sizer    = createIconButtons();
    auto version_info_sizer = createVersionInfoSizer();
    detailed_info_sizer     = createDetailedInfo();
    auto seperator          = createSeperator();

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
    sizer->Add(version_info_sizer, 0, wxALIGN_RIGHT, FromDIP(3));
    sizer->Add(detailed_info_sizer, 0, wxALIGN_CENTER | wxALL, FromDIP(10));
    sizer->Add(seperator, 0, wxEXPAND | wxTOP, FromDIP(5));

    SetSizer(sizer);

    updateStates();
    Layout();

    timer->Bind(wxEVT_TIMER, [&](wxTimerEvent& evt) { onTimer(evt); });

    ready = true;
}

ZaxeDevice::~ZaxeDevice()
{
    if (timer->IsRunning()) {
        timer->Stop();
    }
}

void ZaxeDevice::onTimer(wxTimerEvent& event)
{
    if (nm)
        printing_time_val->SetLabel(get_time_hms(wxDateTime::Now().GetTicks() - nm->attr->start_time) + " / " + nm->attr->estimated_time);
}

wxSizer* ZaxeDevice::createHeader()
{
    model_btn = new Button(this, wxString("UNKNOWN", wxConvUTF8), "", wxBORDER_NONE, FromDIP(24));
    model_btn->SetPaddingSize(wxSize(4, 3));
    model_btn->SetTextColor(*wxWHITE);
    model_btn->SetBackgroundColor(gray500);
    wxGetApp().UpdateDarkUI(model_btn);
    model_btn->Show(!is_expanded);

    model_btn_expanded = new Button(this, wxString("UNKNOWN", wxConvUTF8), "", wxBORDER_NONE, FromDIP(24));
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

    register_to_me_btn = new Button(this, "", "zaxe_add_circle_gray", wxBORDER_NONE, FromDIP(24));
    register_to_me_btn->SetPaddingSize(wxSize(3, 3));
    wxGetApp().UpdateDarkUI(register_to_me_btn);
    register_to_me_btn->SetToolTip("Register To Me");

    wxString nm_switch_icon = (isRemoteDevice()) ? "zaxe_cloud_on" : "zaxe_cloud_off";
    nm_switch_btn           = new Button(this, "", nm_switch_icon, wxBORDER_NONE, FromDIP(24));
    nm_switch_btn->SetPaddingSize(wxSize(3, 3));
    wxGetApp().UpdateDarkUI(nm_switch_btn);

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
    sizer->Add(register_to_me_btn, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(3));
    sizer->Add(nm_switch_btn, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(3));
    sizer->Add(expand_btn, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(3));

    device_name->Bind(wxEVT_LEFT_UP, [&](auto& evt) {
        toggleDeviceNameWidgets();
        device_name_ctrl->SetFocus();
        device_name_ctrl->SetInsertionPointEnd();
    });

    device_name_ctrl->Bind(wxEVT_CHAR_HOOK, [&](auto& evt) {
        if (evt.GetKeyCode() == WXK_ESCAPE) {
            setName();
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

    register_to_me_btn->Bind(wxEVT_BUTTON, [&](auto& e) {
        RegisterPrinterDialog dlg(this, wxID_ANY, nm->name);
        if (dlg.ShowModal() == wxID_OK) {
            auto evt = new wxCommandEvent(EVT_MACHINE_REGISTER);
            wxQueueEvent(nm.get(), evt);
        }
        e.Skip();
    });

    nm_switch_btn->Bind(wxEVT_BUTTON, [&](auto& e) {
        auto evt = new wxCommandEvent(EVT_MACHINE_SWITCH);
        wxQueueEvent(nm.get(), evt);
        e.Skip();
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
        GetParent()->FitInside();
        GetParent()->Layout();
    });

    return sizer;
}

void ZaxeDevice::createAvatar()
{
    avatar_rect = new RoundedRectangle(this, gray200, wxDefaultPosition, wxSize(FromDIP(75), FromDIP(75)), FromDIP(8), 1);
    wxGetApp().UpdateDarkUI(avatar_rect);
    default_avatar = create_scaled_bitmap(get_cover_file_name(), avatar_rect, 48);
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

    status_desc_icon->Bind(wxEVT_BUTTON, [this](auto&) {
        if (update_available && nm->hasRemoteUpdate() && upstream_version.has_value()) {
            confirm([&] { nm->updateFirmware(); },
                    wxString::Format(_L("Current version: %s, Latest Version: %s. Do you want to update your printer?"),
                                     nm->attr->firmware_version.to_string_sf(), upstream_version.value().to_string_sf()));
        }
    });

    return sizer;
}

wxSizer* ZaxeDevice::createPrintButton()
{
    print_btn_mode = PrintBtnMode::Prepare;
    print_btn      = new Button(this, _L("Prepare"));
    print_btn->SetPaddingSize(wxSize(8, 5));
    print_btn->SetBackgroundColor(*wxWHITE);
    auto color = StateColor(std::pair<wxColour, int>(gray300, StateColor::Disabled), std::pair<wxColour, int>(blue500, StateColor::Normal));
    print_btn->SetBorderColor(color);
    print_btn->SetTextColor(color);
    wxGetApp().UpdateDarkUI(print_btn);

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(print_btn, 0, wxALIGN_CENTER | wxALL, FromDIP(1));

    print_btn->Bind(wxEVT_BUTTON, [this](auto& evt) {
        BOOST_LOG_TRIVIAL(info) << "Print button pressed on " << nm->name;
        print_btn->Enable(false);

        if (print_btn_mode == PrintBtnMode::Prepare) {
            evt.SetClientData(nm.get());
            wxPostEvent(GetParent(), evt);
        } else {
            auto net_manager = dynamic_cast<ZaxeNetworkMachineManager*>(GetParent()->GetParent());
            if (net_manager) {
                auto archive = net_manager->get_archive(nm->canPrintMultiPlate(), true);
                if (archive) {
                    print(archive);
                }
            }
        }

        print_btn->Enable(true);
    });

    print_btn->Show(nm->canSendZaxeFile());

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
    wxGetApp().UpdateDarkUI(progress_label);

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

    pause_btn   = create_icon_btn("zaxe_pause", _L("Pause"), [&](const auto&) { confirm([&] { nm->pause(); }); });
    resume_btn  = create_icon_btn("zaxe_resume", _L("Resume"), [&](const auto&) { confirm([&] { nm->resume(); }); });
    stop_btn    = create_icon_btn("zaxe_stop", _L("Stop"), [&](const auto&) { confirm([&] { nm->cancel(); }); });
    preheat_btn = create_icon_btn("zaxe_preheat", _L("Preheat"), [&](const auto&) { confirm([&] { nm->togglePreheat(); }); });
    say_hi_btn  = create_icon_btn("zaxe_hello", _L("Say Hi!"), [&](const auto&) { confirm([&] { nm->sayHi(); }); });
    unload_btn  = create_icon_btn("zaxe_unload", _L("Unload filament"), [&](const auto&) { confirm([&] { nm->unloadFilament(); }); });

    toggle_leds_btn = create_icon_btn(nm->states->leds_switched_on ? "zaxe_lights_on" : "zaxe_lights_off", _L("Toggle Leds"),
                                      [&](const auto& evt) {
                                          if (nm->states->leds_switched_on) {
                                              confirm([&] { nm->toggleLeds(); },
                                                      _L("Turning off the LEDs will disable error detection (if it is on), are you sure?"));
                                          } else {
                                              nm->toggleLeds();
                                          }
                                      });
    toggle_leds_btn->Show(nm->canToggleLeds());

    camera_btn = create_icon_btn("zaxe_cam", _L("Camera"), [&](auto& e) {
        nm->switchOnCam();
        e.Skip();
    });
    camera_btn->Show(nm->hasCam());

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(pause_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(resume_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(stop_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(preheat_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(say_hi_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(unload_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(toggle_leds_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Add(camera_btn, 1, wxRIGHT, FromDIP(5));
    sizer->Layout();
    return sizer;
}

wxSizer* ZaxeDevice::createVersionInfoSizer()
{
    version = new Label(this, nm->attr->firmware_version.to_string_sf(), wxALIGN_RIGHT);
    version->SetForegroundColour(gray500);
    version->SetFont(::Label::Body_10);
    wxGetApp().UpdateDarkUI(version);

    auto sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(version);
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
    printing_file_val = create_label(normal_font, "");
    printing_file_val->Wrap(FromDIP(200));
    setPrintingFile();

    printing_time     = create_label(bold_font, _L("Elapsed / Estimated time"));
    printing_time_val = create_label(normal_font, "");

    auto material = create_label(bold_font, _L("Material"));
    material_val  = create_label(normal_font, "");

    auto nozzle = create_label(bold_font, _L("Nozzle"));
    nozzle_val  = create_label(normal_font, nm->attr->is_lite ? "-" : nm->attr->nozzle + "mm");

    auto nozzle_temp = create_label(bold_font, _L("Nozzle Temp."));
    nozzle_temp_val  = create_label(normal_font, "");

    auto plate_temp = create_label(bold_font, _L("Plate Temp."));
    plate_temp_val  = create_label(normal_font, "");

    identifier     = create_label(bold_font, "");
    identifier_val = create_label(normal_font, "");
    updateIdentifier();

    auto remaining_filament = create_label(bold_font, _L("Remaining Filament"));
    remaining_filament_val  = create_label(normal_font, get_remaining_filament());

    auto sizer = new wxFlexGridSizer(7, 2, FromDIP(5), FromDIP(15));
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
    sizer->Add(identifier, 0, wxEXPAND);
    sizer->Add(identifier_val, 0, wxEXPAND);
    sizer->Add(remaining_filament, 0, wxEXPAND);
    sizer->Add(remaining_filament_val, 0, wxEXPAND);

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

void ZaxeDevice::onUpdateDevice(wxCommandEvent& e)
{
    auto event = e.GetString();

    if (event == "states_update" || event == "hello") {
        updateStates();
    } else if (event == "print_progress" || event == "temperature_progress" || event == "calibration_progress" ||
               event == "upload_progress") {
        updateStates();
        updateProgressValue();
    } else if (event == "new_name") {
        setName();
    } else if (event == "material_change") {
        setMaterialLabel(nm->attr->material_label);
    } else if (event == "nozzle_change") {
        setNozzle();
    } else if (event == "pin_change") {
        setPin(nm->attr->has_pin);
    } else if (event == "start_print") {
        setPrintingFile();
    } else if (event == "file_init_failed") {
        onPrintDenied();
    } else if (event == "temperature_update") {
        onTemperatureUpdate();
    } else if (event == "upload_done") {
        onUploadDone();
    }

    e.Skip();
}

void ZaxeDevice::onClose(wxCommandEvent& e)
{
    updateStates();
    auto evt = new wxCommandEvent(EVT_MACHINE_SWITCH);
    wxQueueEvent(nm.get(), evt);
    e.Skip();
}

void ZaxeDevice::updateStates()
{
    updateModel();
    setName();
    updateRegisterToMeButton();
    updateNetworkType();
    updateProgressLine();
    updateTimer();
    updatePrintButton();
    updateStatusText();
    updateAvatar();
    updateIconButtons();
    setPrintingFile();
    setFilamentPresent();
    setNozzle();
    updateIdentifier();
    updateVersion();

    Layout();
    Refresh();
}

void ZaxeDevice::updateNetworkType()
{
    auto agent = wxGetApp().getAgent();
    if (agent && agent->is_user_login()) {
        wxString _icon    = "zaxe_cloud_off";
        wxString _tooltip = "Local Connection";
        if (isRemoteDevice()) {
            _icon    = "zaxe_cloud_on";
            _tooltip = "Cloud Connection";
        }

        nm_switch_btn->SetToolTip(_tooltip);
        nm_switch_btn->SetIcon(_icon);
    } else {
        nm_switch_btn->Hide();
    }
}

void ZaxeDevice::updateRegisterToMeButton()
{
    bool show = false;

    auto agent = wxGetApp().getAgent();
    if (agent && agent->is_user_login() && !isRemoteDevice()) {
        auto net_manager = dynamic_cast<ZaxeNetworkMachineManager*>(GetParent()->GetParent());
        if (net_manager) {
            show = !net_manager->hasRemoteMachine(nm->attr->serial_no);
        }
    }

    register_to_me_btn->Show(show);
}

void ZaxeDevice::updateProgressLine()
{
    bool show = nm->isAlive() && nm->states->is_busy() && !nm->states->has_error && !nm->states->updating_fw;
    progress_line->Show(show);
    updateProgressValue();

    if (nm->states->calibrating) {
        progress_bar->SetProgressBackgroundColour(progress_calib_color);
    } else if (nm->states->uploading_zaxe_file) {
        progress_bar->SetProgressBackgroundColour(progress_uploading_color);
    } else if (nm->states->heating) {
        progress_bar->SetProgressBackgroundColour(progress_danger_color);
    } else {
        progress_bar->SetProgressBackgroundColour(blue500);
    }
    wxGetApp().UpdateDarkUI(progress_bar);
}

void ZaxeDevice::updateTimer()
{
    if (!nm->states->bed_occupied && !nm->states->heating && !nm->states->paused && nm->states->printing) {
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
    is_print_btn_visible = nm->isAlive() && nm->canSendZaxeFile() && !nm->states->is_busy() && !nm->states->bed_occupied &&
                           !nm->states->has_error;
    print_btn->Show(is_print_btn_visible);
}

void ZaxeDevice::updateStatusText()
{
    wxString title      = "";
    wxString desc       = "";
    wxString desc_color = gray700;
    wxString desc_icon  = "";
    update_available    = false;

    if (!nm->isAlive()) {
        title = _L("Offline");
    } else if (nm->states->updating_fw) {
        title = _L("Updating");
    } else if (nm->states->bed_occupied) {
        title = _L("Bed is occupied");
    } else if (nm->states->calibrating) {
        title = _L("Calibrating");
    } else if (nm->states->heating) {
        title = _L("Heating");
    } else if (nm->states->paused) {
        title = _L("Paused");
    } else if (nm->states->printing) {
        title = _L("Printing");
    } else if (nm->states->uploading_zaxe_file) {
        title = _L("Uploading");
    } else if (!nm->states->is_busy()) {
        title = _L("Ready to use");
    }

    if (isRemoteDevice() && !nm->isAlive()) {
        desc = _L("No connection!");
    } else if (nm->states->has_error) {
        desc       = _L("Device is in error state!");
        desc_color = "#E22005";
        desc_icon  = "zaxe_red_warning";
    } else if (!nm->states->is_busy() && nm->states->bed_occupied) {
        desc       = _L("Please take your print!");
        desc_color = progress_success_color;
    } else if (nm->states->printing) {
        desc = _L("Processing");
    } else if (nm->states->uploading_zaxe_file) {
        desc = _L("Please wait...");
    } else if (nm->hasRemoteUpdate() && !nm->states->is_busy() && upstream_version.has_value() &&
               upstream_version > nm->attr->firmware_version) {
        desc             = _L("Update available");
        desc_color       = "#F4B617";
        desc_icon        = "zaxe_warning_1";
        update_available = true;
    } else if (nm->states->updating_fw) {
        desc       = _L("In progress");
        desc_color = blue500;
    }

    status_title->SetLabel(title);
    status_title->Show(!title.empty());

    if (!desc_icon.empty()) {
        status_desc_icon->SetIcon(desc_icon);
    }
    status_desc_icon->Show(!desc_icon.empty());

    status_desc->SetLabel(desc);
    status_desc->SetForegroundColour(desc_color);
    wxGetApp().UpdateDarkUI(status_desc);
    status_desc->Show(!desc.empty());
}

void ZaxeDevice::updateAvatar()
{
    if (nm->hasSnapshot()) {
        if (nm->states->heating || nm->states->printing || nm->states->calibrating || nm->states->bed_occupied) {
            nm->downloadAvatar();
        } else {
            avatar->SetBitmap(default_avatar);
            avatar_rect->Layout();
            avatar_rect->Refresh();
        }
    }
}

void ZaxeDevice::updateIconButtons()
{
    pause_btn->Show(nm->isAlive() && !nm->states->updating_fw && nm->states->printing && !nm->states->paused && !nm->states->heating);
    resume_btn->Show(nm->isAlive() && !nm->states->updating_fw && nm->states->printing && nm->states->paused && !nm->states->heating);
    stop_btn->Show(nm->isAlive() && !nm->states->updating_fw && nm->states->is_busy() &&
                   (nm->states->printing || !nm->states->uploading_zaxe_file));

    preheat_btn->Show(nm->isAlive() && !nm->states->is_busy() && !nm->states->bed_occupied && !nm->states->has_error);
    preheat_btn->SetIcon(nm->states->preheating ? "zaxe_preheat_active" : "zaxe_preheat");

    say_hi_btn->Show(nm->isAlive() && !nm->states->is_busy() && !nm->states->bed_occupied && !nm->states->has_error);

    unload_btn->Show(nm->isAlive() && nm->canUnloadFilament() && !nm->states->is_busy() && !nm->states->bed_occupied &&
                     !nm->states->has_error);

    toggle_leds_btn->SetIcon(nm->states->leds_switched_on ? "zaxe_lights_on" : "zaxe_lights_off");
    toggle_leds_btn->Show(nm->isAlive() && !nm->states->updating_fw && nm->canToggleLeds());

    camera_btn->Show(nm->isAlive() && nm->hasCam());
}

void ZaxeDevice::updatePrintInfo()
{
    printing_file->Show(is_expanded && nm->states->printing);
    printing_file_val->Show(is_expanded && nm->states->printing);

    printing_time->Show(is_expanded && nm->states->printing);
    printing_time_val->Show(is_expanded && nm->states->printing);
}

void ZaxeDevice::updateVersion() { version->SetLabel(nm->attr->firmware_version.to_string_sf()); }

void ZaxeDevice::onTemperatureUpdate()
{
    nozzle_temp_val->SetLabel(wxString::Format("%.1f %sC", nm->attr->nozzle_temp, wxString::FromUTF8("\xC2\xB0")));
    plate_temp_val->SetLabel(wxString::Format("%.1f %sC", nm->attr->bed_temp, wxString::FromUTF8("\xC2\xB0")));

    Refresh();
}

void ZaxeDevice::updateIdentifier()
{
    if (isRemoteDevice()) {
        identifier->SetLabel(_L("Serial Number"));
        identifier_val->SetLabel(nm->attr->serial_no);
    } else {
        auto _lnm = std::dynamic_pointer_cast<ZaxeLocalMachine>(nm);
        if (_lnm) {
            identifier->SetLabel(_L("IP Address"));
            identifier_val->SetLabel(_lnm->get_ip());
        }
    }
}

void ZaxeDevice::updateModel()
{
    std::string model_str = boost::to_upper_copy(nm->attr->device_model);
    boost::replace_all(model_str, "PLUS", "+");
    model_btn->SetLabel(wxString(model_str.c_str(), wxConvUTF8));
    model_btn_expanded->SetLabel(wxString(model_str.c_str(), wxConvUTF8));
}

void ZaxeDevice::onAvatarReady(wxCommandEvent& e)
{
    if (nm) {
        auto scaled_avatar = nm->getAvatar().ConvertToImage().Scale(FromDIP(60), FromDIP(60), wxIMAGE_QUALITY_HIGH);
        avatar->SetBitmap(scaled_avatar);
        avatar_rect->Layout();
        avatar_rect->Refresh();
    }
    e.Skip();
}

void ZaxeDevice::onPrintButtonStateChanged(bool print_enable, std::shared_ptr<ZaxeArchive> archive)
{
    if (!print_enable) {
        print_btn_mode = PrintBtnMode::Prepare;
    } else {
        std::vector<string> sPV;
        split(sPV, GUI::wxGetApp().preset_bundle->printers.get_selected_preset().name, boost::is_any_of("-"));
        string pN = sPV[0]; // ie: Zaxe Z3S - 0.6mm nozzle -> Zaxe Z3S
        string dM = boost::to_upper_copy(nm->attr->device_model);
        boost::replace_all(dM, "PLUS", "+");
        auto s = pN.find(dM);
        boost::trim(pN);

        std::string sliced_info_model{};
        std::string sliced_info_nozzle{};
        std::string sliced_info_material{};
        if (archive) {
            sliced_info_model    = archive->get_info("model");
            sliced_info_nozzle   = archive->get_info("nozzle_diameter");
            sliced_info_material = archive->get_info("material");
        } else {
            auto& cfg            = wxGetApp().plater()->get_partplate_list().get_curr_plate()->fff_print()->full_print_config();
            sliced_info_model    = ZaxeConfigHelper::get_printer_model(cfg).first;
            sliced_info_nozzle   = ZaxeConfigHelper::get_nozzle(cfg);
            sliced_info_material = ZaxeConfigHelper::get_material(cfg);
        }

        std::string model_nozzle_attr = dM + " " + nm->attr->nozzle;
        std::string model_nozzle_arch = sliced_info_model + " " + sliced_info_nozzle;

        if ((s == std::string::npos || pN.length() != dM.length() + s) ||
            (!nm->attr->is_lite && nm->states->filament_present && nm->attr->material != "custom" &&
             nm->attr->material.compare(sliced_info_material) != 0) ||
            (!nm->attr->is_lite && !case_insensitive_compare(model_nozzle_attr, model_nozzle_arch))) {
            print_btn_mode = PrintBtnMode::Prepare;
        } else {
            print_btn_mode = PrintBtnMode::Print;
        }
    }

    print_btn->SetLabel(print_btn_mode == PrintBtnMode::Prepare ? _L("Prepare") : L("Print now"));
    Layout();
    Refresh();
}

void ZaxeDevice::onPrintDenied()
{
    // Todo: uploading is already false, dialog is never constructed
    if (nm->states->uploading_zaxe_file) {
        RichMessageDialog dialog(GetParent(), _L("Failed to start printing"), _L("XDesktop: Unknown error"), wxICON_ERROR);
        dialog.ShowModal();

        nm->states->uploading_zaxe_file = false;
        updateStates();
    }
}

bool ZaxeDevice::isBusy() { return nm->states->is_busy(); }

void ZaxeDevice::setName()
{
    device_name->SetLabel(wxString(nm->name.c_str(), wxConvUTF8));
    device_name_ctrl->SetValue(wxString(nm->name.c_str(), wxConvUTF8));
}

wxString ZaxeDevice::getName() const { return device_name->GetLabel(); }

void ZaxeDevice::applyDeviceName()
{
    auto val = device_name_ctrl->GetValue().Strip(wxString::both);
    if (val.empty()) {
        setName();
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
    if (nm->hasFilamentPresentInfo() && !nm->states->filament_present) {
        material_val->SetLabel(_L("Not installed"));
    } else {
        material_val->SetLabel(wxString(material_label.c_str(), wxConvUTF8));
    }
    remaining_filament_val->SetLabel(get_remaining_filament());
    Refresh();
}

void ZaxeDevice::setFilamentPresent() { setMaterialLabel(nm->attr->material_label); }

void ZaxeDevice::setPin(bool has_pin) { nm->attr->has_pin = has_pin; }

void ZaxeDevice::setNozzle()
{
    nozzle_val->SetLabel(nm->attr->nozzle + "mm");
    Refresh();
}

void ZaxeDevice::setPrintingFile() { printing_file_val->SetLabel(wxString(nm->attr->printing_file.c_str(), wxConvUTF8)); }

bool ZaxeDevice::has(const wxString& search_text)
{
    auto txt = search_text.Lower();

    return getName().Lower().Find(txt) != wxNOT_FOUND || material_val->GetLabel().Lower().Find(txt) != wxNOT_FOUND ||
           identifier_val->GetLabel().Lower().Find(txt) != wxNOT_FOUND || nozzle_val->GetLabel().Lower().Find(txt) != wxNOT_FOUND ||
           model_btn->GetLabel().Lower().Find(txt) != wxNOT_FOUND || version->GetLabelText().Lower().Find(txt) != wxNOT_FOUND;
}

bool ZaxeDevice::print(std::shared_ptr<ZaxeArchive> archive)
{
    if (!is_print_btn_visible) {
        return false;
    }

    if (archive->support_multiplate() && !nm->canPrintMultiPlate()) {
        wxMessageBox(_L("To support multi plate print, update your printer"), _L("Feature not supported"), wxICON_ERROR);
        return false;
    }

    std::vector<string> sPV;
    split(sPV, GUI::wxGetApp().preset_bundle->printers.get_selected_preset().name, boost::is_any_of("-"));
    string pN = sPV[0]; // ie: Zaxe Z3S - 0.6mm nozzle -> Zaxe Z3S
    string dM = boost::to_upper_copy(nm->attr->device_model);
    boost::replace_all(dM, "PLUS", "+");
    auto s = pN.find(dM);
    boost::trim(pN);

    std::string model_nozzle_attr = dM + " " + nm->attr->nozzle;
    std::string model_nozzle_arch = archive->get_info("model") + " " + archive->get_info("nozzle_diameter");

    if (is_there(nm->attr->device_model, {"x3"}) && !nm->states->usb_present) {
        wxMessageBox(_L("Please insert a usb stick before start printing."), _L("USB stick not found"), wxICON_ERROR);
        return false;
    }

    if (s == std::string::npos || pN.length() != dM.length() + s) {
        wxMessageBox(_L("Device model does NOT match. Please reslice with "
                        "the correct model."),
                     _L("Wrong device model"), wxOK | wxICON_ERROR);
        return false;
    }

    if (!nm->attr->is_lite && nm->states->filament_present && nm->attr->material != "custom" &&
        nm->attr->material.compare(archive->get_info("material")) != 0) {
        BOOST_LOG_TRIVIAL(warning) << "Wrong material type, filament_present: " << nm->states->filament_present
                                   << " deviceMaterial: " << nm->attr->material << " slicerMaterial: " << archive->get_info("material");
        wxMessageBox(_L("Materials don't match with this device. Please "
                        "reslice with the correct material."),
                     _L("Wrong material type"), wxICON_ERROR);
        return false;
    }

    if (!nm->states->filament_present && nm->hasFilamentPresentInfo()) {
        bool confirmed = false;
        confirm([&] { confirmed = true; }, _L("No filament sensed. Do you really want to continue "
                                              "printing?"));
        if (!confirmed)
            return false;
    }

    if (!nm->attr->is_lite && !case_insensitive_compare(model_nozzle_attr, model_nozzle_arch)) {
        BOOST_LOG_TRIVIAL(warning) << "Wrong nozzle type, deviceNozzle: " << model_nozzle_attr << " slicerNozzle: " << model_nozzle_arch;
        wxMessageBox(_L("Currently installed nozzle on device doesn't match with this "
                        "slice. Please reslice with the correct nozzle."),
                     _L("Wrong nozzle type"), wxICON_ERROR);
        return false;
    }

    if (nm->states->bed_dirty) {
        bool confirmed = false;
        confirm([&] { confirmed = true; }, _L("Bed might not be ready for the next print. Please be "
                                              "sure it is clean before pressing YES!"));
        if (!confirmed)
            return false;
    }

    std::thread t([&, archive_path = archive->get_path()]() {
        BOOST_LOG_TRIVIAL(info) << "Print started for " << nm->name;
        if (nm->attr->is_lite) {
            this->nm->upload(wxGetApp().plater()->get_gcode_path().c_str(),
                             translate_chars(wxGetApp().plater()->get_filename().ToStdString()).c_str());
        } else {
            this->nm->upload(archive_path.c_str());
        }
    });
    t.detach(); // crusial. otherwise blocks main thread.
    return true;
}

void ZaxeDevice::onUploadDone()
{
    updateStates();
    wxGetApp().plater()->get_notification_manager()->push_notification(NotificationType::CustomNotification,
                                                                       NotificationManager::NotificationLevel::PrintInfoNotificationLevel,
                                                                       _u8L("Your print job has been sent to the device. Printing will "
                                                                            "start shortly."));
}

void ZaxeDevice::onVersionCheck(const std::map<std::string, Semver>& latest_versions)
{
    if (auto it = latest_versions.find(nm->attr->device_model); it != latest_versions.end()) {
        upstream_version = it->second;
        updateStates();
    }
}

std::string ZaxeDevice::get_cover_file_name()
{
    auto model_str = boost::to_lower_copy(nm->attr->device_model);
    boost::replace_all(model_str, "plus", "+");
    if (!nm->hasPrinterCover() || model_str == "z3s" || model_str == "z3+")
        model_str = "z3";

    return "zaxe_printer_" + model_str;
}

wxString ZaxeDevice::get_remaining_filament()
{
    return (nm->states->filament_present && nm->attr->has_nfc_spool) ? wxString::Format("~%dm", nm->attr->remaining_filament) : "N/A";
}

bool ZaxeDevice::isRemoteDevice() const
{
    auto _rnm = std::dynamic_pointer_cast<ZaxeRemoteMachine>(nm);
    return _rnm != nullptr;
}

} // namespace Slic3r::GUI
