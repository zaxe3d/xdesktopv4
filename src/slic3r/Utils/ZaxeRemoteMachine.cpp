#include "ZaxeRemoteMachine.hpp"

#include "../GUI/GUI_App.hpp"

namespace Slic3r {

ZaxeRemoteMachine::ZaxeRemoteMachine(const std::string& serial) : ZaxeNetworkMachine{}, stateTimer{new wxTimer()}
{
    attr->serial_no = serial;

    stateTimer->Bind(wxEVT_TIMER, [&](wxTimerEvent& _e) { onStateTimer(_e); });
    stateTimer->Start(3000);
    auto initial_event = wxTimerEvent();
    onStateTimer(initial_event);
}

ZaxeRemoteMachine::~ZaxeRemoteMachine()
{
    if (stateTimer->IsRunning()) {
        stateTimer->Stop();
    }
}

void ZaxeRemoteMachine::init(const std::string& content)
{
    try {
        auto j             = nlohmann::json::parse(content);
        name               = j.value("name", "Unknown");
        attr->device_model = j.value("model", "N/A");

        std::vector<std::string> fwV;
        split(fwV, boost::to_lower_copy(j.value("version", "1.0.0")), boost::is_any_of("."));
        attr->firmware_version = Semver(stoi(fwV[0]), stoi(fwV[1]), stoi(fwV[2]));
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(warning) << "Remote device init error: " << e.what() << " - content: " << content;
    }
}

void ZaxeRemoteMachine::onStateTimer(wxTimerEvent& e)
{
    if (prev_state == State::UNKNOWN) {
        prev_state = State::OFFLINE;
    } else if (!isAlive() && prev_state == State::ONLINE) {
        auto evt = new wxCommandEvent(EVT_MACHINE_CLOSE);
        wxQueueEvent(this, evt);
        prev_state = State::OFFLINE;
    } else if (isAlive() && prev_state == State::OFFLINE) {
        auto evt = new wxCommandEvent(EVT_MACHINE_UPDATE);
        evt->SetString("hello");
        wxQueueEvent(this, evt);
        prev_state = State::ONLINE;
    }

    if (!hello_received) {
        GUI::wxGetApp().CallAfter([&] { requestHelloMessage(); });
    }
    e.Skip();
}

bool ZaxeRemoteMachine::isAlive() const
{
    int  max_alive_period_sec = 10;
    auto currentTime          = std::chrono::steady_clock::now();
    auto duration             = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastEventTime);
    return hello_received && max_alive_period_sec > duration.count();
}

void ZaxeRemoteMachine::onWSRead(const std::string& message)
{
    lastEventTime = std::chrono::steady_clock::now();
    handle_device_message(message);
}

void ZaxeRemoteMachine::unloadFilament() { send_command("filament_unload"); }

void ZaxeRemoteMachine::sayHi() { send_command("say_hi"); }

void ZaxeRemoteMachine::togglePreheat() { send_command("toggle_preheat"); }

void ZaxeRemoteMachine::toggleLeds() { send_command("toggle_leds"); }

void ZaxeRemoteMachine::cancel() { send_command("cancel"); }

void ZaxeRemoteMachine::pause() { send_command("pause"); }

void ZaxeRemoteMachine::resume() { send_command("resume"); }

void ZaxeRemoteMachine::startStreaming() { send_command("start_streaming"); }

void ZaxeRemoteMachine::stopStreaming() { send_command("stop_streaming"); }

void ZaxeRemoteMachine::requestHelloMessage() { send_command("send_hello"); }

void ZaxeRemoteMachine::changeName(const char* new_name)
{
    nlohmann::json data;
    data["request"] = "change_name";
    data["name"]    = new_name;
    send(data.dump());
}

void ZaxeRemoteMachine::updateFirmware() { send_command("fw_update"); }

void ZaxeRemoteMachine::send_command(const std::string& command)
{
    nlohmann::json data;
    data["request"] = command;
    send(data.dump());
}

void ZaxeRemoteMachine::send(const std::string& message)
{
    auto agent = Slic3r::GUI::wxGetApp().getAgent();
    if (agent) {
        agent->send_message_to_zaxe_printer(attr->serial_no, message);
    }
}

void ZaxeRemoteMachine::upload(const char* filepath, const char* /*uploadAs*/)
{
    auto agent = Slic3r::GUI::wxGetApp().getAgent();
    if (agent) {
        states->uploading_zaxe_file = true;
        auto evt                    = new wxCommandEvent(EVT_MACHINE_UPDATE);
        evt->SetString("states_update");
        wxQueueEvent(this, evt);

        agent->send_print_job_to_zaxe_printer(attr->serial_no, filepath);
    }
}

void ZaxeRemoteMachine::switchOnCam()
{
    BOOST_LOG_TRIVIAL(error) << "Trying to open remote camera stream on: " << name;

    auto agent = Slic3r::GUI::wxGetApp().getAgent();
    if (agent && attr->firmware_version >= Semver(3, 3, 80)) {
        startStreaming();
        wxFileName ffplay(wxStandardPaths::Get().GetExecutablePath());
        wxString   curExecPath(ffplay.GetPath());

        wxString tool_path =
#ifdef _WIN32
            wxString::Format("%s\\ffplay.exe", curExecPath);
#else
            wxString::Format("%s/ffplay", curExecPath);
#endif
        agent->connect_to_printer_cam(attr->serial_no, attr->device_model, name, tool_path.ToStdString());
    } else {
        wxMessageBox("Need device firmware version at least v3.3.80 to comply.", "Need firmware update for this feautre.",
                     wxICON_INFORMATION);
    }
}
} // namespace Slic3r