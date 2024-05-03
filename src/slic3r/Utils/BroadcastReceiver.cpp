#include "BroadcastReceiver.hpp"

namespace Slic3r {
wxDEFINE_EVENT(EVT_BROADCAST_RECEIVED, wxCommandEvent);

BroadcastReceiver::BroadcastReceiver() {
    wxSocketBase::Initialize();
    m_localAddress.AnyAddress(); // bind to any address (0.0.0.0)
    m_localAddress.Service(BROADCAST_PORT);

    m_socket = new wxDatagramSocket(m_localAddress, wxSOCKET_REUSEADDR);
    m_socket->SetNotify(wxSOCKET_INPUT_FLAG);
    m_socket->SetEventHandler(*this, UDP_SOCKET);
    m_socket->Notify(true);

    Connect(UDP_SOCKET, wxEVT_SOCKET, wxSocketEventHandler(BroadcastReceiver::onBroadcastReceivedEvent));

    if (!m_socket->IsOk()) {
        BOOST_LOG_TRIVIAL(warning) << "BroadcastReceiver: Cannot bind to broadcast socket.";
        return;
    }

    BOOST_LOG_TRIVIAL(info) << boost::format("BroadcastReceiver - Started listenning on port [%1%].") % BROADCAST_PORT;
}

BroadcastReceiver::~BroadcastReceiver()
{
    delete m_socket;
}

void BroadcastReceiver::onBroadcastReceivedEvent(wxSocketEvent& event)
{
    try {
        wxIPV4address addr;
        addr.Service(BROADCAST_PORT);
        char buf[BUFFER_SIZE];
        size_t n;
        switch(event.GetSocketEvent()) {
            case wxSOCKET_INPUT: {
                m_socket->Notify(false);
                n = m_socket->RecvFrom(addr, buf, sizeof(buf)).LastCount();
                if (!n) {
                    BOOST_LOG_TRIVIAL(warning) << "BroadcastReceiver - Failed to receive data.";
                    return;
                }
                auto s = wxString::FromUTF8(buf, n); // TODO use this on received event.
                //BOOST_LOG_TRIVIAL(debug) << boost::format("BroadcastReceiver - Received: %1%. from: %2%") % s % addr.IPAddress();
                m_socket->Notify(true);
                wxCommandEvent event(EVT_BROADCAST_RECEIVED);
                event.SetEventObject(this);
                event.SetString(s);
                wxPostEvent(this, event);
                break;
            }
            default:
                BOOST_LOG_TRIVIAL(warning) << "BroadcastReceiver - onBroadcastReceivedEvent: Unexpected event!.";
        }
    } catch(exception const& e) {
        BOOST_LOG_TRIVIAL(info) << boost::format("BroadcastReceiver - onBroadcastReceivedEvent: Exception occured: [%1%].") % e.what();
    }
}
} // namespace Slic3r
