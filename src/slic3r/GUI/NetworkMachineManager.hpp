#pragma once

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>

#include "../Utils/BroadcastReceiver.hpp"
#include "../Utils/NetworkMachine.hpp"
#include "Widgets/Button.hpp"

#include "ZaxeDevice.hpp"

#include <memory>

namespace Slic3r {

class BroadcastReceiver; // forward declaration.

namespace GUI {
class NetworkMachineManager : public wxPanel
{
public:
    NetworkMachineManager(wxWindow* parent, wxSize size);
    virtual ~NetworkMachineManager() = default;

    void enablePrintNowButton(bool enable);
    void addMachine(std::string ip, int port, std::string id);
    void onModeChanged();

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

    std::unique_ptr<BroadcastReceiver>       broadcast_receiver;
    std::unique_ptr<NetworkMachineContainer> network_machine_container;

    // UI
    Button*     available_btn{nullptr};
    Button*     busy_btn{nullptr};
    Button*     all_btn{nullptr};
    wxPanel*    scrolled_area{nullptr};
    wxBoxSizer* warning_sizer{nullptr};
    wxTextCtrl* search_textctrl{nullptr};

    std::unordered_map<std::string, ZaxeDevice*> device_map;

    bool        print_enable{false};
    FilterState filter_state{FilterState::SHOW_ALL};
};
} // namespace GUI
} // namespace Slic3r
