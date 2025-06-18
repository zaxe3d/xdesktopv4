#ifndef slic3r_BroadcastReceiver_hpp_
#define slic3r_BroadcastReceiver_hpp_

#include <boost/log/trivial.hpp>

#include <wx/app.h>
#include <wx/socket.h>
#include <wx/event.h>

#define BROADCAST_PORT 9295
#define BUFFER_SIZE 1024

using namespace std;

namespace Slic3r {
enum
{
    UDP_SOCKET = 200
};

wxDECLARE_EVENT(EVT_BROADCAST_RECEIVED, wxCommandEvent);

class BroadcastReceiver : public wxEvtHandler
{
public:
    BroadcastReceiver();
    ~BroadcastReceiver();

    void onBroadcastReceivedEvent(wxSocketEvent& event);
private:

    wxIPV4address m_localAddress;
    wxDatagramSocket* m_socket;
};
} // namespace Slic3r
#endif // slic3r_BroadcastReceiver_hpp
