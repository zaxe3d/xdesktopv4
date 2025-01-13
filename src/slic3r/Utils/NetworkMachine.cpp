///|/ Copyright (c) Zaxe 2018 - 2024 Gökhan Öniş @GO
///|/
///|/ XDesktop is released under the terms of the AGPLv3 or higher
///|/
#include "NetworkMachine.hpp"
#include <libslic3r/Utils.hpp>
#include "Http.hpp"
#include "../GUI/GUI_App.hpp"
#include "../GUI/NotificationManager.hpp"

namespace fs = boost::filesystem;

namespace {
void _push_notification(const wxString& text)
{
    Slic3r::GUI::wxGetApp()
        .plater()
        ->get_notification_manager()
        ->push_notification(Slic3r::GUI::NotificationType::CustomNotification,
                            Slic3r::GUI::NotificationManager::NotificationLevel::PrintInfoShortNotificationLevel,
                            wxString::Format(_L("%s command has been sent to printer!"), text).ToStdString());
}
} // namespace

namespace Slic3r {
wxDEFINE_EVENT(EVT_MACHINE_OPEN, MachineEvent);
wxDEFINE_EVENT(EVT_MACHINE_CLOSE, MachineEvent);
wxDEFINE_EVENT(EVT_MACHINE_NEW_MESSAGE, MachineNewMessageEvent);
wxDEFINE_EVENT(EVT_MACHINE_AVATAR_READY, wxCommandEvent);

NetworkMachine::NetworkMachine(string ip, int port, string name, wxEvtHandler* hndlr)
    : ip(ip)
    , port(port)
    , name(name)
    , m_evtHandler(hndlr)
    , attr(new MachineAttributes())
    , states(new MachineStates())
    , upload_progress_info{std::make_shared<UploadProgressInfo>()}
{}

void NetworkMachine::run()
{
    net::io_context ioc; // The io_context is required for websocket I/O
    auto ws = make_shared<Websocket>(ip, port, ioc); // Run the websocket for this machine.
    ws->addReadEventHandler(boost::bind(&NetworkMachine::onWSRead, this, _1));
    ws->addConnectEventHandler(boost::bind(&NetworkMachine::onWSConnect, this));
    ws->addErrorEventHandler(boost::bind(&NetworkMachine::onWSError, this, _1));
    m_ws = ws.get();
    m_running = true;
    ws->run();
    ioc.run(); // Run the I/O service.
}

void NetworkMachine::onWSConnect()
{
}

void NetworkMachine::onWSError(string message)
{
    BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - WS error: %1% on machine [%2% - %3%].") % message % name % ip;
    if (!m_running) return;
    MachineEvent evt(EVT_MACHINE_CLOSE, this, wxID_ANY);
    evt.SetEventObject(this->m_evtHandler);
    wxPostEvent(this->m_evtHandler, evt);
}

void NetworkMachine::onWSRead(string message)
{
    //BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine onReadWS: %1%") % message;
    if (!m_running) return;

    stringstream jsonStream;
    jsonStream.str(message);
    ptree pt; // construct root obj.
    read_json(jsonStream, pt);

    auto get_material_label = [&]() {
        if (pt.get_optional<std::string>("material_label").is_initialized()) {
            return pt.get<string>("material_label", "zaxe_abs");
        }

        std::map<std::string, std::string> materialMap =
            {{"zaxe_abs", "Zaxe ABS"},
             {"zaxe_pla", "Zaxe PLA"},
             {"zaxe_flex", "Zaxe FLEX"},
             {"zaxe_petg", "Zaxe PETG"},
             {"custom", "Custom"}};

        auto material = to_lower_copy(pt.get<string>("material", "zaxe_abs"));

        if (auto iter = materialMap.find(material); iter != materialMap.end())
            return iter->second;
        return material;
    };

    try {
        auto event = pt.get<string>("event");
        if (event == "ping" || event == "temperature_change") return; // ignore...
        //BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine event. [%1%:%2% - %3%]") % name % ip % event;
        if (event == "hello") {
            //name = pt.get<string>("name", name); // already got this from broadcast receiver. might be good for static ip.
            attr->device_model = to_lower_copy(pt.get<string>("device_model", "x1"));
            attr->material = to_lower_copy(pt.get<string>("material", "zaxe_abs"));
            attr->material_label = get_material_label();
            attr->nozzle = pt.get<string>("nozzle", "0.4");
            attr->is_lite = is_there(attr->device_model, {"lite", "x3"});
            attr->is_http = pt.get<string>("protocol", "") == "http";
            attr->is_none_TLS = is_there(attr->device_model, {"z2", "z3", "z4", "x4"}) || attr->is_lite;
            // printing
            attr->printing_file = pt.get<string>("filename", "");
            attr->elapsed_time = pt.get<float>("elapsed_time", 0);
            attr->estimated_time = pt.get<string>("estimated_time", "");
            attr->start_time = wxDateTime::Now().GetTicks() - attr->elapsed_time;
            if (!attr->is_lite) {
                attr->has_pin = to_lower_copy(pt.get<string>("has_pin", "false")) == "true";
                attr->has_nfc_spool = to_lower_copy(pt.get<string>("has_nfc_spool", "false")) == "true";
                attr->filament_color = to_lower_copy(pt.get<string>("filament_color", "unknown"));
                attr->remaining_filament = pt.get<int>("filament_remaining", 0);
            }
            vector<string> fwV;
            split(fwV, to_lower_copy(pt.get<string>("version", "1.0.0")), is_any_of("."));
            attr->firmware_version = wxVersionInfo("v", stoi(fwV[0]), stoi(fwV[1]), stoi(fwV[2]));
        }
        if (event == "hello" || event == "states_update") {
            auto _calibrating  = states->ptreeStringtoBool(pt, "is_calibrating");
            auto _bed_occupied = states->ptreeStringtoBool(pt, "is_bed_occupied");
            auto _bed_dirty    = states->ptreeStringtoBool(pt, "is_bed_dirty");
            auto _printing     = states->ptreeStringtoBool(pt, "is_printing");
            auto _heating      = states->ptreeStringtoBool(pt, "is_heating");
            if (_calibrating != states->calibrating || _bed_occupied != states->bedOccupied || _bed_dirty != states->bedDirty ||
                _printing != states->printing || _heating != states->heating) {
                downloadAvatar();
            }

            states->calibrating    = _calibrating;
            states->bedOccupied    = _bed_occupied;
            states->bedDirty       = _bed_dirty;
            states->usbPresent     = states->ptreeStringtoBool(pt, "is_usb_present");
            states->preheat        = states->ptreeStringtoBool(pt, "is_preheat");
            states->printing       = _printing;
            states->heating        = _heating;
            states->paused         = states->ptreeStringtoBool(pt, "is_paused");
            states->hasError       = states->ptreeStringtoBool(pt, "is_error");
            states->ledsSwitchedOn = states->ptreeStringtoBool(pt, "is_leds");
            states->updatingFw     = states->ptreeStringtoBool(pt, "is_downloading");
            states->has_update     = states->ptreeStringtoBool(pt, "has_update");
            states->filamentPresent= attr->firmware_version.GetMinor() >= 3 && attr->firmware_version.GetMinor() >= 5 // Z3 and FW>=3.5
                                         ? states->ptreeStringtoBool(pt, "is_filament_present") : true;
        }

        if (event == "print_progress" || event == "temperature_progress" || event == "calibration_progress") {
            progress                    = static_cast<int>(pt.get<float>("progress", 0.f));
            states->uploading_zaxe_file = false;
        } else if (event == "upload_done") {
            states->uploading_zaxe_file = false;
        }

        if (event == "new_name")
            name = pt.get<string>("name", "Zaxe");
        if (event == "material_change") {
            attr->material = to_lower_copy(pt.get<string>("material", "zaxe_abs"));
            attr->material_label = get_material_label();
        }
        if (event == "nozzle_change")
            attr->nozzle = pt.get<string>("nozzle", "0.4");
        if (event == "pin_change")
            attr->has_pin = to_lower_copy(pt.get<string>("has_pin", "false")) == "true";
        if (event == "start_print") {
            attr->printing_file = pt.get<string>("filename", "");
            attr->elapsed_time = pt.get<float>("elapsed_time", 0);
            attr->start_time = wxDateTime::Now().GetTicks() - attr->elapsed_time;
            attr->estimated_time = pt.get<string>("estimated_time", "");
        }
        if (event == "spool_data_change") {
            attr->has_nfc_spool = to_lower_copy(pt.get<string>("has_nfc_spool", "false")) == "true";
            attr->filament_color = to_lower_copy(pt.get<string>("filament_color", "unknown"));
        }
        if (event == "temperature_update") {
            attr->nozzle_temp        = pt.get<float>("ext_temp", 0);
            attr->target_nozzle_temp = pt.get<float>("ext_temp_set", 0);
            attr->bed_temp           = pt.get<float>("bed_temp", 0);
            attr->target_bed_temp    = pt.get<float>("bed_temp_set", 0);
        }
        if (event == "hello") { // gather up all the events up untill here.
            MachineEvent evt(EVT_MACHINE_OPEN, this, wxID_ANY); // ? get window id here ?; // ? get window id here ?
            evt.SetEventObject(this->m_evtHandler);
            wxPostEvent(this->m_evtHandler, evt);
        } else {
            MachineNewMessageEvent evt(EVT_MACHINE_NEW_MESSAGE, event, pt, this, wxID_ANY); // ? get window id here ?
            evt.SetEventObject(this->m_evtHandler);
            wxPostEvent(this->m_evtHandler, evt);
        }
    } catch(...) {
        BOOST_LOG_TRIVIAL(warning) << "Cannot parse machine message json.";
    }
}

void NetworkMachine::unloadFilament()
{
    request("filament_unload");
    _push_notification(_u8L("Filament Unload"));
}

void NetworkMachine::sayHi()
{
    request("say_hi");
    _push_notification(_u8L("Say Hi"));
}

void NetworkMachine::cancel(const std::string& pin)
{
    ptree pt;
    pt.put("request", "cancel");
    pt.put("pin", pin);
    send(pt);
    _push_notification(_u8L("Cancel"));
}

void NetworkMachine::pause()
{
    request("pause");
    _push_notification(_u8L("Pause"));
}

void NetworkMachine::resume()
{
    request("resume");
    _push_notification(_u8L("Resume"));
}

void NetworkMachine::togglePreheat()
{
    request("toggle_preheat");
    _push_notification(_u8L("Toggle Preheat"));
}

void NetworkMachine::toggleLeds()
{
    request("toggle_leds");
    _push_notification(_u8L("Toggle Leds"));
}

void NetworkMachine::changeName(const char *new_name)
{
    BOOST_LOG_TRIVIAL(info) << "Changing device name from " << name << " to " << new_name;
    ptree pt; // construct root obj.
    pt.put("request", "change_name");
    pt.put("name", new_name);
    send(pt);
    _push_notification(_u8L("Change Name"));
}

void NetworkMachine::fw_update()
{
    request("fw_update");
    _push_notification(_u8L("Firmware Update"));
}

void NetworkMachine::request(const char* command)
{
    ptree pt; // construct root obj.
    pt.put("request", command);
    send(pt);
}

void NetworkMachine::send(ptree pt)
{
    stringstream ss;
    json_parser::write_json(ss, pt);
    m_ws->send(ss.str());
}

NetworkMachine::~NetworkMachine()
{
    delete attr;
    delete states;
    //delete m_ws;
    if (runnerThread.joinable())
        runnerThread.join();
    if (ftpThread.joinable())
        ftpThread.join();
}

void NetworkMachine::downloadAvatar()
{
    if (!m_running) return;

    ftpThread = boost::thread(&NetworkMachine::ftpRun, this);
    ftpThread.detach();
}

struct response {
    char *memory;
    size_t size;
};

static size_t mem_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct response *mem = static_cast<response*>(userp);

    void *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) { /* out of memory! */
        BOOST_LOG_TRIVIAL(warning) << "Networkmachine - not enough memory (realloc returned NULL)";
        return 0;
    }

    mem->memory = static_cast<char*>(ptr);
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void NetworkMachine::ftpRun()
{
    std::lock_guard<std::mutex> guard(m_ftp_mtx);

    struct response chunk = {0};

    Http::tls_global_init();
    auto curl = curl_easy_init();

    if (!curl) return;

    std::string url = "ftp://" + ip + ":" + std::to_string(m_ftpPort) + "/snapshot.png";
    ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_cb);
    ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&chunk));
    ::curl_easy_setopt(curl, CURLOPT_VERBOSE, get_logging_level() >= 5);
    ::curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    ::curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
    ::curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
    auto res = curl_easy_perform(curl);

    if (CURLE_OK != res) {
        ::curl_easy_cleanup(curl);
        BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - Couldn't connect to machine [%1% - %2%] for downloading avatar.") % name % ip;
        return;
    }

    wxMemoryInputStream s (chunk.memory, chunk.size);
    m_avatar = wxBitmap(wxImage(s, wxBITMAP_TYPE_PNG));

    if (this->m_running && m_avatar.IsOk()) {
        wxCommandEvent evt(EVT_MACHINE_AVATAR_READY, wxID_ANY);
        evt.SetString(this->ip);
        evt.SetEventObject(this->m_evtHandler);
        wxPostEvent(this->m_evtHandler, evt);
    }
    ::curl_easy_cleanup(curl);
}

size_t file_read_cb(char *buffer, size_t size, size_t nitems, void *userp)
{
    auto stream = reinterpret_cast<fs::ifstream*>(userp);

    try {
        stream->read(buffer, size * nitems);
    } catch (const std::exception &) {
        return CURL_READFUNC_ABORT;
    }
    return stream->gcount();
}

int xfercb(void *userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    auto self = static_cast<NetworkMachine*>(userp);

    bool send_event = false;
    if (ultotal <= 0.0 && self->upload_progress_info->progress != 0) {
        self->upload_progress_info->progress         = 0;
        self->upload_progress_info->total_size       = "";
        self->upload_progress_info->transferred_size = "";
        send_event                                   = true;
    } else {
        int progress = (int) (((double) ulnow / (double) ultotal) * 100);
        if (progress == 0 || progress != self->upload_progress_info->progress) {
            auto size_formatted = [](auto completed_size_in_bytes, auto total_size_in_bytes) {
                const double B          = 1.0;
                const double KB         = B * 1024.0;
                const double MB         = KB * 1024.0;
                const double GB         = MB * 1024.0;
                double       _completed = static_cast<double>(completed_size_in_bytes);
                double       _total     = static_cast<double>(total_size_in_bytes);
                double       unit_size  = B;
                std::string  unit       = "B";
                if (total_size_in_bytes >= GB) {
                    unit_size = GB;
                    unit      = "GB";
                } else if (total_size_in_bytes >= MB) {
                    unit_size = MB;
                    unit      = "MB";
                } else if (total_size_in_bytes >= KB) {
                    unit_size = KB;
                    unit      = "KB";
                }
                std::ostringstream c_oss;
                c_oss << std::fixed << std::setprecision(2) << static_cast<double>(_completed) / unit_size << " " << unit;
                auto               completed_str = c_oss.str();
                std::ostringstream t_oss;
                t_oss << std::fixed << std::setprecision(2) << static_cast<double>(_total) / unit_size << " " << unit;
                auto total_str = t_oss.str();
                return std::make_pair(completed_str, total_str);
            }(ulnow, ultotal);

            double upload_speed = 0.0;
            curl_easy_getinfo(self->curl_handle, CURLINFO_SPEED_UPLOAD, &upload_speed);
            auto speed_formatted = [](auto speed) {
                std::ostringstream speed_oss;
                const double       KBps       = 1024.0;
                const double       MBps       = KBps * 1024.0;
                std::string        speed_unit = "B/s";
                if (speed >= MBps) {
                    speed /= MBps;
                    speed_unit = "MB/s";
                } else if (speed >= KBps) {
                    speed /= KBps;
                    speed_unit = "KB/s";
                }
                speed_oss << std::fixed << std::setprecision(2) << speed << " " << speed_unit;
                return speed_oss.str();
            }(upload_speed);

            self->upload_progress_info->progress         = progress;
            self->upload_progress_info->total_size       = size_formatted.second;
            self->upload_progress_info->transferred_size = size_formatted.first;
            self->upload_progress_info->transfer_speed   = speed_formatted;
            send_event                                   = true;
        }

        if (send_event) {
            GUI::wxGetApp().CallAfter([self]() {
                MachineNewMessageEvent evt(EVT_MACHINE_NEW_MESSAGE, "upload_progress", {}, self, wxID_ANY);
                evt.SetEventObject(self->m_evtHandler);
                wxPostEvent(self->m_evtHandler, evt);
            });
        }
    }
    return 0;
}

void NetworkMachine::uploadHTTP(const char* filename, const char* uploadAs)
{
    xfercb(this, 0.0, 0.0, 0.0, 0.0);
    states->uploading_zaxe_file = true;
    std::string url             = "http://" + ip + "/upload.cgi:" + std::to_string(m_httpPort);
    auto        http            = Http::post(std::move(url));
    http.form_add_file("file", filename, uploadAs)
        .on_complete([&](std::string body, unsigned status) {
            states->uploading_zaxe_file = false;
            MachineNewMessageEvent evt(EVT_MACHINE_NEW_MESSAGE, "upload_done", {}, this, wxID_ANY);
            evt.SetEventObject(this->m_evtHandler);
            wxPostEvent(this->m_evtHandler, evt);
        })
        .on_error([&](std::string body, std::string error, unsigned status) {
            states->uploading_zaxe_file = false;
            MachineNewMessageEvent evt(EVT_MACHINE_NEW_MESSAGE, "upload_done", {}, this, wxID_ANY);
            evt.SetEventObject(this->m_evtHandler);
            wxPostEvent(this->m_evtHandler, evt);
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: Error uploading file: %2%, HTTP %3%, "
                                                      "body: `%4%`") %
                                            name % error % status % body;
        })
        .on_progress([&](Http::Progress progress, bool& cancel) {
            xfercb(static_cast<void *>(this), progress.dltotal, progress.dlnow, progress.ultotal, progress.ulnow);
        })
        .perform_sync();
}

void NetworkMachine::uploadFTP(const char *filename, const char *uploadAs)
{
    Http::tls_global_init();
    if (curl_handle) {
        ::curl_easy_reset(curl_handle);
    } else {
        curl_handle = ::curl_easy_init();
    }

    if (!curl_handle) {
        GUI::wxGetApp()
            .plater()
            ->get_notification_manager()
            ->push_notification(GUI::NotificationType::CustomNotification,
                                GUI::NotificationManager::NotificationLevel::WarningNotificationLevel,
                                _u8L("Print cannot be started, internal error."));
        return;
    }

    states->uploading_zaxe_file = true;
    xfercb(this, 0.0, 0.0, 0.0, 0.0);

    fs::path path = fs::path(filename);
    boost::system::error_code ec;
    boost::uintmax_t filesize = file_size(path, ec);
    std::unique_ptr<fs::ifstream> putFile;

    if (!ec) {
        putFile = std::make_unique<fs::ifstream>(path, ios_base::in | ios_base::binary);
        ::curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void *) (putFile.get()));
        ::curl_easy_setopt(curl_handle, CURLOPT_INFILESIZE, filesize);
    }

    std::string pFilename = *uploadAs ? uploadAs : path.filename().string();
    char *encodedFilename = ::curl_easy_escape(curl_handle, pFilename.c_str(), pFilename.length());

    std::string url = "ftp://" + ip + ":" + std::to_string(m_ftpPort) + "/" + std::string(encodedFilename);
    ::curl_easy_setopt(curl_handle, CURLOPT_USERNAME, "zaxe");
    ::curl_easy_setopt(curl_handle, CURLOPT_PASSWORD, "zaxe");
    ::curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, file_read_cb);
    ::curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1L);
    ::curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
    ::curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, get_logging_level() >= 5);
    ::curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, xfercb);
    ::curl_easy_setopt(curl_handle, CURLOPT_PROGRESSDATA, static_cast<void *>(this));
    ::curl_easy_setopt(curl_handle, CURLOPT_FTP_USE_EPSV, 0L);

    if ( ! attr->is_none_TLS) {
        ::curl_easy_setopt(curl_handle, CURLOPT_USE_SSL, CURLUSESSL_CONTROL);
        ::curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        ::curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        ::curl_easy_setopt(curl_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
#ifndef __WINDOWS__
        ::curl_easy_setopt(curl_handle, CURLOPT_SSL_CIPHER_LIST, "AES256-GCM-SHA384");
#endif
    }
    auto res = curl_easy_perform(curl_handle);
    if (CURLE_OK != res) {
        BOOST_LOG_TRIVIAL(warning) << boost::format(
                                          "Networkmachine - Couldn't connect to machine [%1% - %2%] for uploading print. ERROR_CODE: %3%") %
                                          name % ip % res;
        GUI::wxGetApp().plater()->get_notification_manager()->push_notification(GUI::NotificationType::CustomNotification,
                                                                           GUI::NotificationManager::NotificationLevel::WarningNotificationLevel,
                                                                           _u8L("Print cannot be started, internal error."));

        states->uploading_zaxe_file = false;
        MachineNewMessageEvent evt(EVT_MACHINE_NEW_MESSAGE, "states_update", {}, this, wxID_ANY);
        evt.SetEventObject(this->m_evtHandler);
        wxPostEvent(this->m_evtHandler, evt);
    } else {
        states->uploading_zaxe_file = false;
        MachineNewMessageEvent evt(EVT_MACHINE_NEW_MESSAGE, "upload_done", {}, this, wxID_ANY);
        evt.SetEventObject(this->m_evtHandler);
        wxPostEvent(this->m_evtHandler, evt);
    }

    ::curl_free(encodedFilename);
    ::curl_easy_cleanup(curl_handle);
    curl_handle = nullptr;
}

void NetworkMachine::upload(const char *filename, const char *uploadAs)
{
    if (attr->is_http) {
        uploadHTTP(filename, uploadAs);
    } else {
        uploadFTP(filename, uploadAs);
    }
}

NetworkMachineContainer::NetworkMachineContainer() {}

NetworkMachineContainer::~NetworkMachineContainer() {
    boost::lock_guard<boost::mutex> maplock(m_mtx);
    for (auto& it : m_machineMap)
        m_machineMap[it.first]->shutdown(); // shutdown each before clearing.
    m_machineMap.clear();
}

shared_ptr<NetworkMachine> NetworkMachineContainer::addMachine(string ip, int port, string name)
{
    boost::lock_guard<boost::mutex> maplock(m_mtx);
    // If we have it already with the same ip, - do nothing.
    if (m_machineMap.find(string(ip)) != m_machineMap.end()) return nullptr;
    BOOST_LOG_TRIVIAL(info) << boost::format("NetworkMachineContainer - Trying to connect machine: [%1% - %2%].") % name % ip;
    auto nm = make_shared<NetworkMachine>(ip, port, name, this);
    nm->runnerThread = boost::thread(&NetworkMachine::run, nm);
    nm->runnerThread.detach();
    m_machineMap[ip] = nm; // Hold this for the carousel.

    return nm;
}

void NetworkMachineContainer::removeMachine(string ip)
{
    boost::lock_guard<boost::mutex> maplock(m_mtx);
    if (m_machineMap.find(string(ip)) == m_machineMap.end()) return;
    m_machineMap[ip]->shutdown();
    m_machineMap.erase(ip);
}
} // namespace Slic3r
