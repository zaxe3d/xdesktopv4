#include "ZaxeLocalMachine.hpp"

#include "libslic3r/Utils.hpp"
#include "nlohmann/json.hpp"
#include <curl/curl.h>
#include "Http.hpp"
#include "../GUI/GUI_App.hpp"
#include <boost/filesystem.hpp>

namespace Slic3r {

struct response
{
    char*  memory;
    size_t size;
};

static size_t mem_cb(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t           realsize = size * nmemb;
    struct response* mem      = static_cast<response*>(userp);

    void* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) { /* out of memory! */
        BOOST_LOG_TRIVIAL(warning) << "ZaxeLocalMachine - not enough memory (realloc returned NULL)";
        return 0;
    }

    mem->memory = static_cast<char*>(ptr);
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

size_t file_read_cb(char* buffer, size_t size, size_t nitems, void* userp)
{
    auto stream = reinterpret_cast<boost::filesystem::ifstream*>(userp);

    try {
        stream->read(buffer, size * nitems);
    } catch (const std::exception&) {
        return CURL_READFUNC_ABORT;
    }
    return stream->gcount();
}

int xfercb(void* userp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    auto self       = static_cast<ZaxeLocalMachine*>(userp);
    bool send_event = false;
    if (ultotal <= 0.0 && self->upload_progress_info->progress != 0) {
        self->upload_progress_info->progress         = 0;
        self->upload_progress_info->total_size       = "";
        self->upload_progress_info->transferred_size = "";
        send_event                                   = true;
    } else {
        int progress = (int) (((double) ulnow / (double) ultotal) * 100);
        if (progress != self->upload_progress_info->progress) {
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
                auto completed_str = c_oss.str();

                std::ostringstream t_oss;
                t_oss << std::fixed << std::setprecision(2) << static_cast<double>(_total) / unit_size << " " << unit;
                auto total_str = t_oss.str();

                return std::make_pair(completed_str, total_str);
            }(ulnow, ultotal);

            self->upload_progress_info->progress         = progress;
            self->upload_progress_info->total_size       = size_formatted.second;
            self->upload_progress_info->transferred_size = size_formatted.first;
            send_event                                   = true;
        }
    }

    if (send_event) {
        GUI::wxGetApp().CallAfter([self]() {
            auto evt = new wxCommandEvent(EVT_MACHINE_UPDATE);
            evt->SetString("upload_progress");
            wxQueueEvent(self, evt);
        });
    }

    return 0;
}

ZaxeLocalMachine::ZaxeLocalMachine(const std::string& _ip, int _port, const std::string& _name) : ZaxeNetworkMachine{}, ip{_ip}, port{_port}
{
    name = _name;

    worker = std::make_unique<std::thread>([&]() {
        net::io_context ioc;                                              // The io_context is required for websocket I/O
        auto            _ws = std::make_shared<Websocket>(ip, port, ioc); // Run the websocket for this machine.
        _ws->addReadEventHandler(std::bind(&ZaxeLocalMachine::onWSRead, this, std::placeholders::_1));
        _ws->addConnectEventHandler(std::bind(&ZaxeLocalMachine::onWSConnect, this));
        _ws->addErrorEventHandler(std::bind(&ZaxeLocalMachine::onWSError, this, std::placeholders::_1));
        running.store(true);
        ws = _ws.get();
        ws->run();
        ioc.run(); // Run the I/O service.
    });
    worker->detach();
}

void ZaxeLocalMachine::onWSConnect() { BOOST_LOG_TRIVIAL(debug) << __func__ << " Name: " << name << " IP: " << ip; }

void ZaxeLocalMachine::onWSError(const std::string& message)
{
    BOOST_LOG_TRIVIAL(warning) << __func__ << " Name: " << name << " IP: " << ip << " Message: " << message;
    if (!running.load())
        return;
    auto evt = new wxCommandEvent(EVT_MACHINE_CLOSE);
    wxQueueEvent(this, evt);
}

void ZaxeLocalMachine::onWSRead(const std::string& message) { handle_device_message(message); }

void ZaxeLocalMachine::unloadFilament() { send_command("filament_unload"); }

void ZaxeLocalMachine::sayHi() { send_command("say_hi"); }

void ZaxeLocalMachine::togglePreheat() { send_command("toggle_preheat"); }

void ZaxeLocalMachine::toggleLeds() { send_command("toggle_leds"); }

void ZaxeLocalMachine::cancel() { send_command("cancel"); }

void ZaxeLocalMachine::pause() { send_command("pause"); }

void ZaxeLocalMachine::resume() { send_command("resume"); }

void ZaxeLocalMachine::changeName(const char* new_name)
{
    nlohmann::json msg;
    msg["request"] = "change_name";
    msg["name"]    = "new_name";
    send(msg.dump());
}

void ZaxeLocalMachine::updateFirmware() { send_command("fw_update"); }

void ZaxeLocalMachine::send_command(const std::string& command)
{
    nlohmann::json msg;
    msg["request"] = command;
    send(msg.dump());
}

void ZaxeLocalMachine::send(const std::string& message) { ws->send(message); }

void ZaxeLocalMachine::switchOnCam()
{
    BOOST_LOG_TRIVIAL(info) << "Trying to open camera stream on: " << name;
    if (attr->firmware_version >= Semver(3, 3, 80)) {
        wxFileName ffplay(wxStandardPaths::Get().GetExecutablePath());
        wxString   curExecPath(ffplay.GetPath());
#ifdef _WIN32
        wxString ffplay_path = wxString::Format("%s\\ffplay.exe", curExecPath);
        wxString command     = wxString::Format("cmd.exe /c \"\"%s\" tcp://%s:5002 -window_title \"Zaxe %s: %s\" -x 720\"", ffplay_path,
                                                get_ip(), boost::to_upper_copy(attr->device_model), name);
        BOOST_LOG_TRIVIAL(debug) << __func__ << ": " << command.ToStdString();
        wxExecute(command, wxEXEC_ASYNC | wxEXEC_HIDE_CONSOLE);
#else
        wxExecute(curExecPath + "/ffplay tcp://" + get_ip() + ":5002 -window_title \"Zaxe " + boost::to_upper_copy(attr->device_model) +
                      ": " + name + "\" -x 720",
                  wxEXEC_ASYNC);
#endif
    } else {
        wxMessageBox("Need device firmware version at least v3.3.80 to comply.", "Need firmware update for this feautre.",
                     wxICON_INFORMATION);
    }
}

void ZaxeLocalMachine::downloadAvatar()
{
    if (!running.load())
        return;

    avatarThread = std::thread(&ZaxeLocalMachine::downloadSnapshot, this);
    avatarThread.detach();
}

void ZaxeLocalMachine::downloadSnapshot()
{
    std::lock_guard<std::mutex> guard(avatarMtx);

    CURL*           curl;
    CURLcode        res;
    struct response chunk = {0};

    const int max_retries = 3;
    int       retries     = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) {
        BOOST_LOG_TRIVIAL(error) << "Failed to initialize libcurl.";
        return;
    }

    std::string url = "ftp://" + ip + ":" + std::to_string(ftpPort) + "/snapshot.png";

    do {
        ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_cb);
        ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&chunk));
        ::curl_easy_setopt(curl, CURLOPT_VERBOSE, get_logging_level() >= 5);
        ::curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        ::curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
        ::curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);

        res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            BOOST_LOG_TRIVIAL(debug) << "FTP file download successful.";
            break;
        } else {
            BOOST_LOG_TRIVIAL(warning) << boost::format("Attempt %1% failed with error [%2%]: %3%") % (retries + 1) % res %
                                              curl_easy_strerror(res);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    } while (++retries < max_retries);

    if (res != CURLE_OK) {
        BOOST_LOG_TRIVIAL(error) << boost::format("Failed to download file after %1% attempts. Error: %2%") % max_retries %
                                        curl_easy_strerror(res);
        ::curl_easy_cleanup(curl);
        return;
    }

    wxMemoryInputStream s(chunk.memory, chunk.size);
    avatar = wxBitmap(wxImage(s, wxBITMAP_TYPE_PNG));

    if (running.load() && avatar.IsOk()) {
        auto evt = new wxCommandEvent(EVT_MACHINE_AVATAR_READY);
        wxQueueEvent(this, evt);
    }
    ::curl_easy_cleanup(curl);
}

void ZaxeLocalMachine::uploadHTTP(const char* filename, const char* uploadAs)
{
    states->uploading_zaxe_file = true;
    xfercb(this, 0.0, 0.0, 0.0, 0.0);

    std::string url  = "http://" + ip + "/upload.cgi:" + std::to_string(httpPort);
    auto        http = Http::post(std::move(url));
    http.form_add_file("file", filename, uploadAs)
        .on_complete([&](std::string body, unsigned status) {
            states->uploading_zaxe_file = false;
            auto evt                    = new wxCommandEvent(EVT_MACHINE_UPDATE);
            evt->SetString("upload_done");
            wxQueueEvent(this, evt);
        })
        .on_error([&](std::string body, std::string error, unsigned status) {
            states->uploading_zaxe_file = false;
            auto evt                    = new wxCommandEvent(EVT_MACHINE_UPDATE);
            evt->SetString("states_update");
            wxQueueEvent(this, evt);
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: Error uploading file: %2%, HTTP %3%, "
                                                      "body: `%4%`") %
                                            name % error % status % body;
        })
        .on_progress([&](Http::Progress progress, bool& cancel) {
            xfercb(static_cast<void*>(this), progress.dltotal, progress.dlnow, progress.ultotal, progress.ulnow);
        })
        .perform_sync();
}

void ZaxeLocalMachine::uploadFTP(const char* filename, const char* uploadAs)
{
    CURL*    curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl)
        return;

    states->uploading_zaxe_file = true;
    xfercb(this, 0.0, 0.0, 0.0, 0.0);

    namespace fs                       = boost::filesystem;
    fs::path                      path = fs::path(filename);
    boost::system::error_code     ec;
    boost::uintmax_t              filesize = file_size(path, ec);
    std::unique_ptr<fs::ifstream> putFile;

    if (!ec) {
        putFile = std::make_unique<fs::ifstream>(path, ios_base::in | ios_base::binary);
        ::curl_easy_setopt(curl, CURLOPT_READDATA, (void*) (putFile.get()));
        ::curl_easy_setopt(curl, CURLOPT_INFILESIZE, filesize);
    }

    std::string pFilename       = *uploadAs ? uploadAs : path.filename().string();
    char*       encodedFilename = ::curl_easy_escape(curl, pFilename.c_str(), pFilename.length());

    std::string url = "ftp://" + ip + ":" + std::to_string(ftpPort) + "/" + std::string(encodedFilename);
    ::curl_easy_setopt(curl, CURLOPT_USERNAME, "zaxe");
    ::curl_easy_setopt(curl, CURLOPT_PASSWORD, "zaxe");
    ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl, CURLOPT_READFUNCTION, file_read_cb);
    ::curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    ::curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    ::curl_easy_setopt(curl, CURLOPT_VERBOSE, get_logging_level() >= 5);
    ::curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfercb);
    ::curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, static_cast<void*>(this));
    ::curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);

    if (!attr->is_none_TLS) {
        ::curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_CONTROL);
        ::curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        ::curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        ::curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
#ifndef __WINDOWS__
        ::curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST, "AES256-GCM-SHA384");
#endif
    }
    res = curl_easy_perform(curl);
    if (CURLE_OK != res) {
        BOOST_LOG_TRIVIAL(warning)
            << boost::format("ZaxeLocalMachine - Couldn't connect to machine [%1% - %2%] for uploading print. ERROR_CODE: %3%") % name %
                   ip % res;
    }
    ::curl_free(encodedFilename);
    ::curl_easy_cleanup(curl);
    putFile.reset();
    curl_global_cleanup();

    states->uploading_zaxe_file = false;
    auto evt                    = new wxCommandEvent(EVT_MACHINE_UPDATE);
    evt->SetString("upload_done");
    wxQueueEvent(this, evt);
}

void ZaxeLocalMachine::upload(const char* filename, const char* uploadAs)
{
    if (attr->is_http) {
        uploadHTTP(filename, uploadAs);
    } else {
        uploadFTP(filename, uploadAs);
    }
}
} // namespace Slic3r