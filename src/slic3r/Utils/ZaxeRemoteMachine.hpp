#pragma once

#include "ZaxeNetworkMachine.hpp"

#include <chrono>

namespace Slic3r {
class ZaxeRemoteMachine : public ZaxeNetworkMachine
{
public:
    ZaxeRemoteMachine(const std::string& serial);
    ~ZaxeRemoteMachine();

    void init(const std::string& content);

    bool isAlive() const override;

    void onWSRead(const std::string& message) override;

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
    void downloadAvatar() override {}
    
    void startStreaming();
    void stopStreaming();

    void upload(const char* filename, const char* uploadAs = "") override;

private:
    void send_command(const std::string& command);
    void send(const std::string& message) override;
    void onStateTimer(wxTimerEvent& e);

    wxTimer*                              stateTimer;
    std::chrono::steady_clock::time_point lastEventTime;

    enum class State { UNKNOWN, ONLINE, OFFLINE };
    State prev_state{State::UNKNOWN};
};
} // namespace Slic3r