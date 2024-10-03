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
    e.Skip();
}

bool ZaxeRemoteMachine::isAlive() const
{
    int  max_alive_period_sec = 10;
    auto currentTime          = std::chrono::steady_clock::now();
    auto duration             = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastEventTime);
    return max_alive_period_sec > duration.count();
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

void ZaxeRemoteMachine::changeName(const char* new_name)
{
    nlohmann::json data;
    data["request"] = "change_name";
    data["name"]    = new_name;

    nlohmann::json j;
    j["event"]  = "user-command";
    j["serial"] = attr->serial_no;
    j["data"]   = data.dump();

    send(j.dump());
}

void ZaxeRemoteMachine::updateFirmware() { send_command("fw_update"); }

void ZaxeRemoteMachine::send_command(const std::string& command)
{
    nlohmann::json data;
    data["request"] = command;

    nlohmann::json j;
    j["event"]  = "user-command";
    j["serial"] = attr->serial_no;
    j["data"]   = data.dump();

    send(j.dump());
}

void ZaxeRemoteMachine::send(const std::string& message)
{
    auto evt = new wxCommandEvent(EVT_MACHINE_REMOTE_CMD);
    evt->SetString(message);
    wxQueueEvent(this, evt);
}

void ZaxeRemoteMachine::upload(const char* filepath, const char* /*uploadAs*/)
{
    auto agent = Slic3r::GUI::wxGetApp().getAgent();
    if (agent) {
        auto read_binary_file = [](const std::string& file_path) {
            std::ifstream file(file_path, std::ios::binary | std::ios::ate);
            if (!file) {
                throw std::runtime_error("Failed to open file: " + file_path);
            }

            std::streamsize file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> file_content(file_size);
            if (!file.read(reinterpret_cast<char*>(file_content.data()), file_size)) {
                throw std::runtime_error("Failed to read file content: " + file_path);
            }

            return file_content;
        };

        states->uploading_zaxe_file = true;
        auto evt                    = new wxCommandEvent(EVT_MACHINE_UPDATE);
        evt->SetString("states_update");
        wxQueueEvent(this, evt);

        std::string file_name = boost::filesystem::path(filepath).filename().string();
        char file_name_padded[50];
        std::strncpy(file_name_padded, file_name.c_str(), 50);
        std::memset(file_name_padded + file_name.size(), '\0', 50 - file_name.size());

        char serial_padded[20];
        std::strncpy(serial_padded, attr->serial_no.c_str(), 20);
        std::memset(serial_padded + attr->serial_no.size(), '\0', 20 - attr->serial_no.size());

        std::vector<uint8_t> file_content = read_binary_file(filepath);
        std::vector<uint8_t> byte_array;
        byte_array.insert(byte_array.end(), file_name_padded, file_name_padded + 50);
        byte_array.insert(byte_array.end(), serial_padded, serial_padded + 20);
        byte_array.insert(byte_array.end(), file_content.begin(), file_content.end());

        agent->send_print_job_to_zaxe_printer(byte_array);
    }
}
} // namespace Slic3r