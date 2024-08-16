///|/ Copyright (c) Zaxe 2018 - 2024 Gökhan Öniş @GO
///|/
///|/ XDesktop is released under the terms of the AGPLv3 or higher
///|/
#include "WebSocket.hpp"
#include <boost/signals2.hpp>

namespace Slic3r {
Websocket::Websocket(string host, int port, net::io_context& ioc) :
    m_resolver(net::make_strand(ioc)),
    m_ws(net::make_strand(ioc)),
    m_host(host),
    m_port(port) { }

void Websocket::run()
{
    try {
        beast::error_code ec;
        // Look up the domain name
        auto const results = m_resolver.resolve(m_host, std::to_string(m_port), ec);
        if (ec) return onErrorSignal(ec.message());
        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(m_ws).connect(results, ec);
        if (ec) return onErrorSignal(ec.message());
        beast::get_lowest_layer(m_ws).expires_never();
        // Set suggested timeout settings for the websocket
        m_ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
        m_ws.set_option(websocket::stream_base::decorator([](websocket::response_type& req)
                    { req.set(http::field::user_agent,
                            string(BOOST_BEAST_VERSION_STRING) +
                            " websocket-xdesktop"); }));
        m_ws.handshake(m_host, "/", ec); // Perform the websocket handshake
        if (ec) return onErrorSignal(ec.message());

        onConnectSignal(); // signal connected.

        // set timeouts
        m_ws.set_option(stream_base::timeout{
            std::chrono::seconds(30), // handshake timeout
            std::chrono::seconds(30), // idle timeout
            true // internal ping
        });

        // start reading...
        m_ws.async_read(m_buffer, beast::bind_front_handler(&Websocket::onRead, shared_from_this()));
    } catch (...) {
        onErrorSignal("Unknown websocket error.");
    }
}

void Websocket::onRead(beast::error_code ec, size_t bytesTransferred)
{
    try {
        if (ec == websocket::error::closed) return;
        if (ec) onErrorSignal(ec.message());
        else {
            auto message = beast::buffers_to_string(m_buffer.data());
            m_buffer.consume(bytesTransferred); // remove the data that was read.
            //BOOST_LOG_TRIVIAL(debug) << "Websocket - onMachineMessage: " << message;
            onReadSignal(message);
            m_ws.async_read(m_buffer, beast::bind_front_handler(&Websocket::onRead, shared_from_this()));
        }
    } catch(beast::error_code e) {
        onErrorSignal(e.message());
    } catch (...) {
        onErrorSignal("Unknown websocket error.");
    }
}

void Websocket::send(string message)
{
    m_ws.write(net::buffer(message));
}

Websocket::~Websocket()
{
}
} // namespace Slic3r
