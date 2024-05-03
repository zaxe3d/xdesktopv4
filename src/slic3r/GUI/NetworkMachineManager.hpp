#ifndef slic3r_NetworkMachineManager_hpp_
#define slic3r_NetworkMachineManager_hpp_

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string.hpp>

#include "../Utils/BroadcastReceiver.hpp"
#include "../Utils/NetworkMachine.hpp"

#include "Device.hpp"

namespace Slic3r {

class BroadcastReceiver; // forward declaration.

namespace GUI {
class NetworkMachineManager : public wxScrolledWindow
{
public:
    NetworkMachineManager(wxWindow *parent, wxSize size);
    virtual ~NetworkMachineManager(); // avoid leakage on possible children.

    void enablePrintNowButton(bool enable);
    void addMachine(std::string ip, int port, std::string id);
    void onModeChanged();

private:
    // slots
    void onBroadcastReceived(wxCommandEvent &event);
    void onMachineOpen(MachineEvent &event);
    void onMachineClose(MachineEvent &event);
    void onMachineMessage(MachineNewMessageEvent &event);
    void onMachineAvatarReady(wxCommandEvent &event);

    BroadcastReceiver* m_broadcastReceiver;
    NetworkMachineContainer* m_networkMContainer;

    // UI
    wxSizer    *m_mainSizer;
    wxBoxSizer *m_warningSizer;
    wxSizer    *m_searchSizer;
    wxSizer    *m_deviceListSizer;
    wxTextCtrl *m_searchTextCtrl;

    boost::unordered_map<std::string, shared_ptr<Device>> m_deviceMap;

    bool m_printNowButtonEnabled = false;
};
} // namespace Slic3r
} // namespace GUI
#endif
