#pragma once

#include <wx/panel.h>
#include <optional>

#include "Widgets/Button.hpp"
#include "Widgets/TextInput.hpp"

#include "ZaxeDevice.hpp"
#include "../Utils/BroadcastReceiver.hpp"
#include "../Utils/ZaxeLocalMachine.hpp"
#include "../Utils/ZaxeRemoteMachine.hpp"

namespace Slic3r::GUI {
class ZaxeNetworkMachineManager : public wxPanel
{
public:
    enum class PrintMode { SinglePlate, AllPlates };

    ZaxeNetworkMachineManager(wxWindow* parent, wxSize size);
    virtual ~ZaxeNetworkMachineManager();

    wxPanel* scrolledArea() { return scrolled_area; }

    void                         enablePrintNowButton(bool enable);
    std::shared_ptr<ZaxeArchive> get_archive(bool support_multiplate = true, bool force_reset = false);
    bool                         print(ZaxeNetworkMachine* machine, PrintMode mode);

    bool hasRemoteMachine(const std::string& serial_no);

private:
    enum class FilterState { SHOW_AVAILABLE, SHOW_BUSY, SHOW_ALL };
    FilterState filter_state{FilterState::SHOW_ALL};

    std::vector<std::shared_ptr<ZaxeLocalMachine>>  local_machines;
    std::vector<std::shared_ptr<ZaxeRemoteMachine>> remote_machines;
    std::map<std::string, ZaxeDevice*>              device_map;

    BroadcastReceiver broadcast_receiver;

    // UI
    Button*     available_btn{nullptr};
    Button*     busy_btn{nullptr};
    Button*     all_btn{nullptr};
    wxPanel*    scrolled_area{nullptr};
    wxBoxSizer* warning_sizer{nullptr};
    TextInput*  search_ctrl{nullptr};

    bool                         print_enable{false};
    std::shared_ptr<ZaxeArchive> archive{nullptr};

    wxTimer*                      version_check_timer;
    std::map<std::string, Semver> fw_versions;

    wxPanel* createFilterArea();
    wxPanel* createWarningArea();
    wxPanel* createScrolledArea();

    void prepareRemoteDevices(const std::vector<std::pair<std::string, std::string>> &devices);
    void addRemoteDevice(const std::string& serial_no, const std::string& init_msg, bool subscribe);

    void onDeviceDetected(std::shared_ptr<ZaxeNetworkMachine> nm);
    void onBroadcastReceived(wxCommandEvent& event);

    void applyFilters();
    bool prepare_archive(PrintMode mode);

    void checkVersions();

    template<typename Container>
    std::optional<typename Container::value_type> findBySerial(const Container& container, const std::string& serial)
    {
        auto it = std::find_if(container.begin(), container.end(), [&serial](const auto& item) { return item->attr->serial_no == serial; });
        if (it != container.end()) {
            return *it;
        }
        return std::nullopt;
    }
};
} // namespace Slic3r::GUI