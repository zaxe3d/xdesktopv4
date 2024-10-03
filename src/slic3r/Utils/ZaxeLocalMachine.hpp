#pragma once

#include "ZaxeNetworkMachine.hpp"
#include "WebSocket.hpp"

#include <atomic>
#include <thread>

namespace Slic3r {
class ZaxeLocalMachine : public ZaxeNetworkMachine
{
public:
    ZaxeLocalMachine(const std::string& _ip, int _port, const std::string& _name);

    std::string get_ip() const { return ip; }

    void unloadFilament() override;
    void sayHi() override;
    void togglePreheat() override;
    void toggleLeds() override;
    void cancel() override;
    void pause() override;
    void resume() override;
    void changeName(const char* new_name) override;
    void updateFirmware() override;
    void switchOnCam() override;
    void downloadAvatar() override;

    void upload(const char* filename, const char* uploadAs = "") override;

private:
    std::string                  ip;
    int                          port;
    std::unique_ptr<std::thread> worker{nullptr};
    Websocket*                   ws;
    std::atomic<bool>            running{false};

    int ftpPort  = 9494;
    int httpPort = 80;

    std::thread avatarThread;
    std::mutex  avatarMtx;

    void onWSConnect();
    void onWSRead(const std::string& message) override;
    void onWSError(const std::string& message);

    void send_command(const std::string& command);
    void send(const std::string& message) override;

    void downloadSnapshot();

    void uploadHTTP(const char* filename, const char* uploadAs = "");
    void uploadFTP(const char* filename, const char* uploadAs = "");
};
} // namespace Slic3r