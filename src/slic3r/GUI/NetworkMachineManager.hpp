///|/ Copyright (c) Zaxe 2018 - 2024 Gökhan Öniş @GO
///|/
///|/ XDesktop is released under the terms of the AGPLv3 or higher
///|/
#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>

#include "../Utils/BroadcastReceiver.hpp"
#include "../Utils/NetworkMachine.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/TextInput.hpp"
#include "libslic3r/Semver.hpp"
#include "ZaxeDevice.hpp"
#include "libslic3r/Format/ZaxeArchive.hpp"

#include <memory>

namespace Slic3r {

class BroadcastReceiver; // forward declaration.

namespace GUI {
class NetworkMachineManager : public wxPanel
{
public:
    NetworkMachineManager(wxWindow* parent, wxSize size);
    virtual ~NetworkMachineManager();

    void enablePrintNowButton(bool enable);
    void addMachine(std::string ip, int port, std::string id);

    wxPanel* scrolledArea() { return scrolled_area; }
    std::shared_ptr<ZaxeArchive> get_archive(bool support_multiplate = true, bool force_reset = false);

    enum class PrintMode { SinglePlate, AllPlates };
    bool print(NetworkMachine* machine, PrintMode mode);

private:
    enum class FilterState { SHOW_AVAILABLE, SHOW_BUSY, SHOW_ALL };

    wxPanel* createFilterArea();
    wxPanel* createWarningArea();
    wxPanel* createScrolledArea();

    void applyFilters();

    // slots
    void onBroadcastReceived(wxCommandEvent& event);
    void onMachineOpen(MachineEvent& event);
    void onMachineClose(MachineEvent& event);
    void onMachineMessage(MachineNewMessageEvent& event);
    void onMachineAvatarReady(wxCommandEvent& event);

    void checkVersions();
    bool prepare_archive(PrintMode mode);

    std::unique_ptr<BroadcastReceiver>       broadcast_receiver;
    std::unique_ptr<NetworkMachineContainer> network_machine_container;

    // UI
    Button*     available_btn{nullptr};
    Button*     busy_btn{nullptr};
    Button*     all_btn{nullptr};
    wxPanel*    scrolled_area{nullptr};
    wxBoxSizer* warning_sizer{nullptr};
    TextInput*  search_ctrl{nullptr};

    std::unordered_map<std::string, ZaxeDevice*> device_map;

    bool        print_enable{false};
    FilterState filter_state{FilterState::SHOW_ALL};

    wxTimer*                      version_check_timer;
    std::map<std::string, Semver> fw_versions;

    std::shared_ptr<ZaxeArchive> archive{nullptr};
};
} // namespace GUI
} // namespace Slic3r
