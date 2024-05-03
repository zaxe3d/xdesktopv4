#ifndef slic3r_WebSocket_hpp_
#define slic3r_WebSocket_hpp_

#include <boost/signals2.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace sig = boost::signals2;

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
using namespace std;
using namespace beast::websocket;

namespace Slic3r {
class Websocket : public enable_shared_from_this<Websocket>
{
public:
    Websocket(string host, int port, net::io_context& ioc);
    ~Websocket();

    void run(); // run this func in a thread.

    void send(string message); // sends message to websocket.

    // signals
    typedef sig::signal<void ()> ConnectEvent;
    typedef sig::signal<void (string message)> ReadEvent;
    typedef sig::signal<void (string message)> ErrorEvent;

    typedef ReadEvent::slot_type ReadEventHandler;
    typedef ConnectEvent::slot_type ConnectEventHandler;
    typedef ErrorEvent::slot_type ErrorEventHandler;

    sig::connection addReadEventHandler(ReadEventHandler handler) { return onReadSignal.connect(handler); }
    sig::connection addConnectEventHandler(ConnectEventHandler handler) { return onConnectSignal.connect(handler); }
    sig::connection addErrorEventHandler(ErrorEventHandler handler) { return onErrorSignal.connect(handler); }

    ReadEvent onReadSignal;
    ConnectEvent onConnectSignal;
    ErrorEvent onErrorSignal;
private:
    void onRead(beast::error_code ec, size_t bytesTransferred);

    websocket::stream<beast::tcp_stream> m_ws; // socket.
    beast::flat_buffer m_buffer; // buffer.
    tcp::resolver m_resolver;

    string m_host;
    int m_port;
};
} // namespace Slic3r
#endif // slic3r_WebSocket_hpp_
