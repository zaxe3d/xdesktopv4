#include "NetworkMachineManager.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp" // for wxGetApp().app_config
#include "I18N.hpp"
#include "libslic3r/Utils.hpp"
#include "ZaxeDevice.hpp"

#include <wx/artprov.h>

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
    sizer->Add(warning_area, 0, wxEXPAND);
    sizer->Add(scrolled_area, 1, wxEXPAND);

    SetSizer(sizer);
    Layout();

    network_machine_container->Bind(EVT_MACHINE_OPEN, &NetworkMachineManager::onMachineOpen, this);
    network_machine_container->Bind(EVT_MACHINE_CLOSE, &NetworkMachineManager::onMachineClose, this);
    network_machine_container->Bind(EVT_MACHINE_NEW_MESSAGE, &NetworkMachineManager::onMachineMessage, this);
    network_machine_container->Bind(EVT_MACHINE_AVATAR_READY, &NetworkMachineManager::onMachineAvatarReady, this);

    broadcast_receiver->Bind(EVT_BROADCAST_RECEIVED, &NetworkMachineManager::onBroadcastReceived, this);

    // add custom ips here.
    // auto ips = wxGetApp().app_config->get_custom_ips();
    // for (int i = 0; i < ips.size(); i++)
    //    addMachine(ips[i], 9294, "Zaxe (m.)");
}

wxPanel* NetworkMachineManager::createFilterArea()
{
    auto panel = new wxPanel(this);
    panel->SetBackgroundColour(*wxWHITE);

    auto modify_button_color = [this](Button* b, bool is_active) {
        wxString active_fg   = "#FFFFFF";
        wxString inactive_fg = "#98A2B3";
        wxString active_bg   = "#009ADE";
        wxString inactive_bg = "#F2F4F7";
        b->SetBackgroundColor(is_active ? active_bg : inactive_bg);
        b->SetBorderColor(is_active ? active_bg : inactive_bg);
        b->SetTextColor(is_active ? active_fg : inactive_fg);
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
    auto search_bg = wxColor("#F2F4F7");
    search_ctrl->SetBackgroundColor(std::make_pair(search_bg, (int) StateColor::Normal));
    search_ctrl->GetTextCtrl()->SetBackgroundColour(search_bg);
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

    auto warning_icon = new wxStaticBitmap(panel, wxID_ANY, wxArtProvider::GetBitmap(wxART_WARNING), wxDefaultPosition, wxSize(35, 35));

    warning_sizer = new wxBoxSizer(wxHORIZONTAL);
    warning_sizer->AddStretchSpacer(1);
    warning_sizer->Add(warning_icon, 0, wxALIGN_CENTER | wxALL, 5);
    warning_sizer->Add(no_device_found_txt, 0, wxALIGN_CENTER | wxALL, 1);
    warning_sizer->AddStretchSpacer(1);
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
    if (enable == print_enable)
        return;
    for (auto& [ip, dev] : device_map) {
        if (!dev)
            continue;
        dev->enablePrintButton(enable);
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

    auto zd = new ZaxeDevice(event.nm, scrolled_area);
    zd->enablePrintButton(print_enable);
    device_map[event.nm->ip] = zd;
    applyFilters();
    warning_sizer->Show(device_map.empty());
    scrolled_area->GetSizer()->Add(zd, 0, wxEXPAND | wxALL, FromDIP(5));

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

void NetworkMachineManager::onModeChanged()
{
    /*
    std::for_each(m_deviceMap.begin(), m_deviceMap.end(), [](auto& d) {
        if (d.second)
            d.second->onModeChanged();
    });
    */
}

void NetworkMachineManager::applyFilters()
{
    auto searchText = search_ctrl->GetTextCtrl()->GetValue();

    for (auto& [ip, dev] : device_map) {
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
} // namespace Slic3r::GUI
