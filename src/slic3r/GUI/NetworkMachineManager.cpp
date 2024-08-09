#include "NetworkMachineManager.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp" // for wxGetApp().app_config
#include "I18N.hpp"
#include "libslic3r/Utils.hpp"
#include "ZaxeDevice.hpp"
#include "NotificationManager.hpp"

namespace Slic3r::GUI {

NetworkMachineManager::NetworkMachineManager(wxWindow* parent, wxSize size)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
    , broadcast_receiver(std::make_unique<BroadcastReceiver>())
    , network_machine_container(std::make_unique<NetworkMachineContainer>())

{
#ifdef __WINDOWS__
    SetDoubleBuffered(true);
#endif

    SetBackgroundColour(*wxWHITE);

    auto filter_area  = createFilterArea();
    auto warning_area = createWarningArea();
    scrolled_area     = createScrolledArea();

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(filter_area, 0, wxEXPAND | wxTOP, FromDIP(10));
    sizer->Add(warning_area, 0, wxEXPAND | wxTOP, FromDIP(25));
    sizer->Add(scrolled_area, 1, wxEXPAND);

    SetSizer(sizer);
    Layout();

    network_machine_container->Bind(EVT_MACHINE_OPEN, &NetworkMachineManager::onMachineOpen, this);
    network_machine_container->Bind(EVT_MACHINE_CLOSE, &NetworkMachineManager::onMachineClose, this);
    network_machine_container->Bind(EVT_MACHINE_NEW_MESSAGE, &NetworkMachineManager::onMachineMessage, this);
    network_machine_container->Bind(EVT_MACHINE_AVATAR_READY, &NetworkMachineManager::onMachineAvatarReady, this);

    broadcast_receiver->Bind(EVT_BROADCAST_RECEIVED, &NetworkMachineManager::onBroadcastReceived, this);

    checkVersions();
    version_check_timer = new wxTimer();
    version_check_timer->SetOwner(this);
    Bind(wxEVT_TIMER, [&](wxTimerEvent&) { checkVersions(); });
    version_check_timer->Start(1'000 * 60 * 10);
}

NetworkMachineManager::~NetworkMachineManager()
{
    if (version_check_timer) {
        version_check_timer->Stop();
        delete version_check_timer;
        version_check_timer = nullptr;
    }
}

void NetworkMachineManager::checkVersions()
{
    int timeout_sec = 15;
    Http::get("https://software.zaxe.com/z3new/firmware.json")
        .timeout_connect(timeout_sec)
        .timeout_max(timeout_sec)
        .on_error([&](std::string, std::string error, unsigned http_status) {
            BOOST_LOG_TRIVIAL(error) << "Error getting z3 fw version: HTTP(" << http_status << ") " << error;
        })
        .on_complete([&](std::string body, unsigned http_status) {
            if (http_status != 200) {
                BOOST_LOG_TRIVIAL(error) << "Error getting z3 fw version: HTTP(" << http_status << ") " << body;
                return;
            }

            try {
                boost::trim(body);
                boost::property_tree::ptree root;
                std::stringstream           json_stream(body);
                boost::property_tree::read_json(json_stream, root);

                auto fw_latest     = Semver(root.get<std::string>("version"));
                fw_versions["z3"]  = fw_latest;
                fw_versions["z3s"] = fw_latest;

                CallAfter([&]() {
                    for (auto& [ip, dev] : device_map) {
                        if (dev) {
                            dev->onVersionCheck(fw_versions);
                        }
                    }
                });
            } catch (...) {
                BOOST_LOG_TRIVIAL(error) << "Error getting z3 fw version";
            }
        })
        .perform();
}

wxPanel* NetworkMachineManager::createFilterArea()
{
    auto panel = new wxPanel(this);
    panel->SetBackgroundColour(*wxWHITE);

    auto modify_button_color = [](Button* b, bool is_active) {
        wxString active_fg         = "#FFFFFF";
        wxString inactive_fg       = "#98A2B3";
        wxString inactive_hover_fg = "#36BFFA";
        wxString active_bg         = "#009ADE";
        wxString inactive_bg       = "#F2F4F7";

        StateColor active_text_fg(std::pair<wxColour, int>(active_fg, StateColor::Disabled),
                                  std::pair<wxColour, int>(active_fg, StateColor::Pressed),
                                  std::pair<wxColour, int>(active_fg, StateColor::Hovered),
                                  std::pair<wxColour, int>(active_fg, StateColor::Normal));

        StateColor inactive_text_fg(std::pair<wxColour, int>(inactive_fg, StateColor::Disabled),
                                    std::pair<wxColour, int>(inactive_fg, StateColor::Pressed),
                                    std::pair<wxColour, int>(inactive_hover_fg, StateColor::Hovered),
                                    std::pair<wxColour, int>(inactive_fg, StateColor::Normal));

        b->SetBackgroundColor(is_active ? active_bg : inactive_bg);
        b->SetBorderColor(is_active ? active_bg : inactive_bg);
        b->SetTextColor(is_active ? active_text_fg : inactive_text_fg);
    };

    auto create_filter_button = [=](const wxString& label, bool is_active, auto cb) {
        auto* b = new Button(panel, label);
        b->SetMaxSize(wxSize(-1, FromDIP(25)));
        b->SetCornerRadius(FromDIP(11));
        modify_button_color(b, is_active);
        b->Bind(wxEVT_BUTTON, cb);
        return b;
    };

    available_btn = create_filter_button(_L("Available"), false, [=](auto& e) {
        filter_state = FilterState::SHOW_AVAILABLE;
        modify_button_color(available_btn, true);
        modify_button_color(busy_btn, false);
        modify_button_color(all_btn, false);
        applyFilters();
    });
    busy_btn      = create_filter_button(_L("Busy"), false, [=](auto& e) {
        filter_state = FilterState::SHOW_BUSY;
        modify_button_color(available_btn, false);
        modify_button_color(busy_btn, true);
        modify_button_color(all_btn, false);
        applyFilters();
    });
    all_btn       = create_filter_button(_L("All"), true, [=](auto& e) {
        filter_state = FilterState::SHOW_ALL;
        modify_button_color(available_btn, false);
        modify_button_color(busy_btn, false);
        modify_button_color(all_btn, true);
        applyFilters();
    });

    auto btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->AddStretchSpacer(1);
    btn_sizer->Add(available_btn, 5, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, FromDIP(5));
    btn_sizer->Add(busy_btn, 5, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, FromDIP(5));
    btn_sizer->Add(all_btn, 5, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, FromDIP(5));
    btn_sizer->AddStretchSpacer(1);

    search_ctrl = new TextInput(panel, "", "", "zaxe_search", wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, FromDIP(24));
    search_ctrl->SetCornerRadius(FromDIP(10));
    auto gray100 = wxColor("#F2F4F7");
    search_ctrl->SetBackgroundColor(std::make_pair(gray100, (int) StateColor::Normal));
    search_ctrl->GetTextCtrl()->SetBackgroundColour(gray100);
    search_ctrl->GetTextCtrl()->SetMaxLength(50);
    search_ctrl->GetTextCtrl()->SetHint(_L("Search printer, nozzle, material, ip address..."));
    wxGetApp().UpdateDarkUI(search_ctrl);
    wxGetApp().UpdateDarkUI(search_ctrl->GetTextCtrl());
    search_ctrl->GetTextCtrl()->Bind(wxEVT_TEXT, [&](auto& evt) {
        applyFilters();
        evt.Skip();
    });

    auto search_sizer = new wxBoxSizer(wxHORIZONTAL);
    search_sizer->AddStretchSpacer(1);
    search_sizer->Add(search_ctrl, 13);
    search_sizer->AddStretchSpacer(1);

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(btn_sizer, 1, wxEXPAND);
    sizer->Add(search_sizer, 1, wxEXPAND | wxTOP, FromDIP(5));

    panel->SetSizer(sizer);
    panel->Layout();

    return panel;
}

wxPanel* NetworkMachineManager::createWarningArea()
{
    auto panel = new wxPanel(this);

    auto no_device_found_txt = new wxStaticText(panel, wxID_ANY, _L("Can not find a Zaxe on the network"), wxDefaultPosition, wxDefaultSize);
    wxGetApp().UpdateDarkUI(no_device_found_txt);

    wxFont label_font = wxGetApp().normal_font();
    label_font.SetPointSize(14);
    no_device_found_txt->SetFont(label_font);

    auto warning_icon     = create_scaled_bitmap("zaxe_no_connection", panel, 48);
    auto warning_icon_bmp = new wxStaticBitmap(panel, wxID_ANY, warning_icon, wxDefaultPosition, wxDefaultSize);

    warning_sizer = new wxBoxSizer(wxVERTICAL);
    warning_sizer->Add(warning_icon_bmp, 0, wxALIGN_CENTER | wxALL, 5);
    warning_sizer->Add(no_device_found_txt, 0, wxALIGN_CENTER | wxALL, 1);
    warning_sizer->Show(device_map.empty());

    panel->SetSizer(warning_sizer);
    panel->Layout();

    return panel;
}

wxPanel* NetworkMachineManager::createScrolledArea()
{
    auto panel = new wxScrolled<wxPanel>(this);

    auto sizer = new wxBoxSizer(wxVERTICAL);

    panel->SetSizer(sizer);
    panel->SetScrollRate(0, 5);

    panel->Layout();

    return panel;
}

void NetworkMachineManager::enablePrintNowButton(bool enable)
{
    for (auto& [ip, dev] : device_map) {
        if (!dev)
            continue;
        dev->onPrintButtonStateChanged(enable, archive);
    }

    print_enable = enable;
}

void NetworkMachineManager::onBroadcastReceived(wxCommandEvent& event)
{
    std::stringstream jsonStream;
    jsonStream.str(std::string(event.GetString().utf8_str().data())); // wxString to std::string

    boost::property_tree::ptree pt; // construct root obj.
    boost::property_tree::read_json(jsonStream, pt);
    try {
        this->addMachine(pt.get<std::string>("ip"), pt.get<int>("port"), pt.get<std::string>("id"));
    } catch (std::exception ex) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Cannot parse broadcast message json: [%1%].") % ex.what();
    }
}

void NetworkMachineManager::addMachine(std::string ip, int port, std::string id)
{
    auto machine = network_machine_container->addMachine(ip, port, id);
}

void NetworkMachineManager::onMachineOpen(MachineEvent& event)
{
    // Now we can add this to UI.
    if (!event.nm || device_map.find(event.nm->ip) != device_map.end()) {
        return;
    }

    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineManager - Connected to machine: [%1% - %2%].") % event.nm->name % event.nm->ip;

    Freeze();
    auto zd = new ZaxeDevice(event.nm, scrolled_area);
    zd->onPrintButtonStateChanged(print_enable, archive);
    zd->onVersionCheck(fw_versions);
    device_map[event.nm->ip] = zd;
    applyFilters();
    warning_sizer->Show(device_map.empty());
    scrolled_area->GetSizer()->Add(zd, 0, wxEXPAND | wxALL, FromDIP(5));
    Thaw();

    scrolled_area->Layout();
    scrolled_area->FitInside();
    Layout();
}

void NetworkMachineManager::onMachineClose(MachineEvent& event)
{
    if (!event.nm) {
        return;
    }

    auto it = device_map.find(event.nm->ip);
    if (it == device_map.end()) {
        return;
    }

    if (it->second) {
        delete it->second;
        it->second = nullptr;
    }

    if (device_map.erase(event.nm->ip) == 0) {
        return;
    }

    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineManager - Closing machine: [%1% - %2%].") % event.nm->name % event.nm->ip;
    network_machine_container->removeMachine(event.nm->ip);
    warning_sizer->Show(device_map.empty());

    scrolled_area->Layout();
    scrolled_area->FitInside();
    Layout();
}

void NetworkMachineManager::onMachineMessage(MachineNewMessageEvent& event)
{
    if (!event.nm) {
        return;
    }

    auto dev = device_map.find(event.nm->ip);
    if (dev == device_map.end() || dev->second == nullptr) {
        return;
    }

    if (event.event == "states_update") {
        dev->second->updateStates();
    } else if (event.event == "print_progress" || event.event == "temperature_progress" || event.event == "calibration_progress") {
        event.nm->progress = event.pt.get<float>("progress", 0);
        dev->second->updateProgressValue();
    } else if (event.event == "new_name") {
        dev->second->setName(event.nm->name);
    } else if (event.event == "material_change") {
        dev->second->setMaterialLabel(event.nm->attr->materialLabel);
    } else if (event.event == "nozzle_change") {
        dev->second->setNozzle(event.nm->attr->nozzle);
    } else if (event.event == "pin_change") {
        dev->second->setPin(event.nm->attr->hasPin);
    } else if (event.event == "start_print") {
        dev->second->setFileStart();
    } else if (event.event == "file_init_failed") {
        dev->second->onPrintDenied();
    } else if (event.event == "temperature_update") {
        dev->second->onTemperatureUpdate();
    } else if (event.event == "upload_done") {
        dev->second->onUploadDone();
    }
}

void NetworkMachineManager::onMachineAvatarReady(wxCommandEvent& event)
{
    string ip = event.GetString().ToStdString();
    auto   it = device_map.find(ip);
    if (it == device_map.end()) {
        return;
    }
    // BOOST_LOG_TRIVIAL(debug) << boost::format("NetworkMachineManager - Avatar ready on machine: [%1%].") % ip;
    it->second->onAvatarReady();
}

void NetworkMachineManager::applyFilters()
{
    auto searchText = search_ctrl->GetTextCtrl()->GetValue();

    for (auto& [ip, dev] : device_map) {
        if (!dev) {
            continue;
        }

        bool show = true;
        if (filter_state == FilterState::SHOW_AVAILABLE) {
            show = show && !dev->isBusy();
        } else if (filter_state == FilterState::SHOW_BUSY) {
            show = show && dev->isBusy();
        }

        show = show && dev->has(searchText);

        dev->Show(show);
    }

    scrolled_area->Layout();
    scrolled_area->FitInside();
    Layout();
}

bool NetworkMachineManager::prepare_archive(PrintMode mode)
{
    auto& partplate_list    = wxGetApp().plater()->get_partplate_list();
    int   empty_plate_count = 0;
    for (int i = 0; i < partplate_list.get_plate_count(); i++) {
        if (partplate_list.get_plate(i)->empty()) {
            empty_plate_count++;
        }
    }

    int printable_plate_count = partplate_list.get_plate_count() - empty_plate_count;
    if (printable_plate_count == 0) {
        BOOST_LOG_TRIVIAL(error) << __func__ << ": Nothing to print!";
        return false;
    }

    if (printable_plate_count == 1) {
        mode = PrintMode::SinglePlate;
    }

    archive.reset();

    if (GUI::wxGetApp().preset_bundle->printers.is_selected_preset_zaxe()) {
        bool is_multi_plate = mode == PrintMode::AllPlates;

        auto get_date_time = []() {
            std::time_t        now = std::time(nullptr);
            std::tm*           lt  = std::localtime(&now);
            std::ostringstream ss;
            ss << std::put_time(lt, "%Y%m%d_%H%M%S");
            return ss.str();
        };

        std::string multi_plate_file_name = (boost::format("xdesktop_%1%") % get_date_time()).str();
        std::string single_plate_file_name{};
        if (!is_multi_plate) {
            for (int i = 0; i < partplate_list.get_plate_count(); i++) {
                auto plate = partplate_list.get_plate(i);
                if (!plate->empty()) {
                    single_plate_file_name = boost::filesystem::path(plate->fff_print()->output_filename("")).stem().string();
                }
            }
        }

        std::string file_name      = (is_multi_plate || single_plate_file_name.empty()) ? multi_plate_file_name : single_plate_file_name;
        std::string file_extension = is_multi_plate ? "zaxemp" : "zaxe";
        boost::filesystem::path _temp_path(wxStandardPaths::Get().GetTempDir().utf8_str().data());
        boost::filesystem::path archive_path(_temp_path);
        archive_path /= (boost::format("%1%.%2%") % file_name % file_extension).str();

        archive    = std::make_shared<ZaxeArchive>(archive_path.string(), is_multi_plate);
        auto model = GUI::wxGetApp().preset_bundle->printers.get_selected_preset().name;

        for (int i = 0; i < partplate_list.get_plate_count(); i++) {
            if (mode != PrintMode::AllPlates && partplate_list.get_curr_plate_index() != i) {
                continue;
            }
            auto plate = partplate_list.get_plate(i);
            if (plate->empty()) {
                BOOST_LOG_TRIVIAL(warning) << __func__ << ": Plate " << i << " is empty, skipping...";
                continue;
            }
            auto fff_print = plate->fff_print();

            auto thumnails  = wxGetApp().plater()->get_thumbnails_list_by_plate_index(i);
            auto model_file = wxGetApp().plater()->get_model_path_by_plate_index(i);
            archive->append(thumnails, *fff_print, plate->get_tmp_gcode_path(), model_file);
        }
        archive->prepare_file();
        return true;
    }

    return false;
}

bool NetworkMachineManager::print(NetworkMachine* machine, PrintMode mode)
{
    auto create_error_notification = []() {
        wxGetApp()
            .plater()
            ->get_notification_manager()
            ->push_notification(NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel,
                                _u8L("Selected printer cannot be found in network, please select a printer from Zaxe Machine Carousel"));
    };

    if (!machine) {
        create_error_notification();
        return false;
    }

    auto it = device_map.find(machine->ip);
    if (it == device_map.end()) {
        create_error_notification();
        return false;
    }

    if (prepare_archive(mode)) {
        enablePrintNowButton(print_enable);
        return it->second->print(archive);
    }

    enablePrintNowButton(print_enable);
    return false;
}

std::shared_ptr<ZaxeArchive> NetworkMachineManager::get_archive(bool support_multiplate)
{
    bool is_all_plates_selected = wxGetApp().plater()->get_preview_canvas3D()->is_all_plates_selected();
    if (!archive || is_all_plates_selected || (archive && (!support_multiplate && archive->support_multiplate()))) {
        auto last_slice_mode = wxGetApp().mainframe->get_last_slice_mode();
        auto mode            = (support_multiplate && (is_all_plates_selected || last_slice_mode == MainFrame::ModeSelectType::eSliceAll)) ?
                                   PrintMode::AllPlates :
                                   PrintMode::SinglePlate;

        if (prepare_archive(mode)) {
            enablePrintNowButton(print_enable);
        }
    }

    return archive;
}
} // namespace Slic3r::GUI
