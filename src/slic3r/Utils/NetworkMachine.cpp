#include "NetworkMachine.hpp"
#include <libslic3r/Utils.hpp>
#include "Http.hpp"

namespace fs = boost::filesystem;

namespace Slic3r {
wxDEFINE_EVENT(EVT_MACHINE_OPEN, MachineEvent);
wxDEFINE_EVENT(EVT_MACHINE_CLOSE, MachineEvent);
wxDEFINE_EVENT(EVT_MACHINE_NEW_MESSAGE, MachineNewMessageEvent);
wxDEFINE_EVENT(EVT_MACHINE_AVATAR_READY, wxCommandEvent);

NetworkMachine::NetworkMachine(string ip, int port, string name, wxEvtHandler* hndlr) :
    ip(ip),
    port(port),
    name(name),
    m_evtHandler(hndlr),
    attr(new MachineAttributes()),
    states(new MachineStates())
{
}

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
            attr->deviceModel = to_lower_copy(pt.get<string>("device_model", "x1"));
            attr->material = to_lower_copy(pt.get<string>("material", "zaxe_abs"));
            attr->materialLabel = get_material_label();
            attr->nozzle = pt.get<string>("nozzle", "0.4");
            attr->hasSnapshot = is_there(attr->deviceModel, {"z1", "z2", "z3"});
            attr->isLite = is_there(attr->deviceModel, {"lite", "x3"});
            attr->isHttp = pt.get<string>("protocol", "") == "http";
            attr->isNoneTLS = is_there(attr->deviceModel, {"z2", "z3"}) || attr->isLite;
            // printing
            attr->printingFile = pt.get<string>("filename", "");
            attr->elapsedTime = pt.get<float>("elapsed_time", 0);
            attr->estimatedTime = pt.get<string>("estimated_time", "");
            attr->startTime = wxDateTime::Now().GetTicks() - attr->elapsedTime;
            if (!attr->isLite) {
                attr->hasPin = to_lower_copy(pt.get<string>("has_pin", "false")) == "true";
                attr->hasNFCSpool = to_lower_copy(pt.get<string>("has_nfc_spool", "false")) == "true";
                attr->filamentColor = to_lower_copy(pt.get<string>("filament_color", "unknown"));
            }
            vector<string> fwV;
            split(fwV, to_lower_copy(pt.get<string>("version", "1.0.0")), is_any_of("."));
            attr->firmwareVersion = wxVersionInfo("v", stoi(fwV[0]), stoi(fwV[1]), stoi(fwV[2]));
        }
        if (event == "hello" || event == "states_update") {
            // states
            states->updating       = states->ptreeStringtoBool(pt, "is_updating");
            states->calibrating    = states->ptreeStringtoBool(pt, "is_calibrating");
            states->bedOccupied    = states->ptreeStringtoBool(pt, "is_bed_occupied");
            states->usbPresent     = states->ptreeStringtoBool(pt, "is_usb_present");
            states->preheat        = states->ptreeStringtoBool(pt, "is_preheat");
            states->printing       = states->ptreeStringtoBool(pt, "is_printing");
            states->heating        = states->ptreeStringtoBool(pt, "is_heating");
            states->paused         = states->ptreeStringtoBool(pt, "is_paused");
            states->hasError       = states->ptreeStringtoBool(pt, "is_error");
            states->filamentPresent= attr->firmwareVersion.GetMinor() >= 3 && attr->firmwareVersion.GetMinor() >= 5 // Z3 and FW>=3.5
                                         ? states->ptreeStringtoBool(pt, "is_filament_present") : true;
        }
        if (event == "new_name")
            name = pt.get<string>("name", "Zaxe");
        if (event == "material_change") {
            attr->material = to_lower_copy(pt.get<string>("material", "zaxe_abs"));
            attr->materialLabel = get_material_label();
        }
        if (event == "nozzle_change")
            attr->nozzle = pt.get<string>("nozzle", "0.4");
        if (event == "pin_change")
            attr->hasPin = to_lower_copy(pt.get<string>("has_pin", "false")) == "true";
        if (event == "start_print") {
            attr->printingFile = pt.get<string>("filename", "");
            attr->elapsedTime = pt.get<float>("elapsed_time", 0);
            attr->startTime = wxDateTime::Now().GetTicks() - attr->elapsedTime;
            attr->estimatedTime = pt.get<string>("estimated_time", "");
        }
        if (event == "spool_data_change") {
            attr->hasNFCSpool = to_lower_copy(pt.get<string>("has_nfc_spool", "false")) == "true";
            attr->filamentColor = to_lower_copy(pt.get<string>("filament_color", "unknown"));
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
}

void NetworkMachine::sayHi()
{
    request("say_hi");
}

void NetworkMachine::cancel()
{
    request("cancel");
}

void NetworkMachine::pause()
{
    request("pause");
}

void NetworkMachine::resume()
{
    request("resume");
}

void NetworkMachine::togglePreheat()
{
    request("toggle_preheat");
}

void NetworkMachine::changeName(const char *new_name)
{
    BOOST_LOG_TRIVIAL(info) << "Changing device name from " << name << " to " << new_name;
    ptree pt; // construct root obj.
    pt.put("request", "change_name");
    pt.put("name", new_name);
    send(pt);
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
    CURL *curl;
    CURLcode res;
    struct response chunk = {0};

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) return;

    std::string url = "ftp://" + ip + ":" + std::to_string(m_ftpPort) + "/snapshot.png";
    ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_cb);
    ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(&chunk));
    ::curl_easy_setopt(curl, CURLOPT_VERBOSE, get_logging_level() >= 5);
    ::curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    ::curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5);
    ::curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0);
    res = curl_easy_perform(curl);

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
    curl_global_cleanup();
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
    if (ultotal <= 0.0) return 0;

    auto self = static_cast<NetworkMachine*>(userp);

    int progress = (int)(((double)ulnow / (double)ultotal) * 100);
    if (progress != self->progress && self->m_uploadProgressCallback != nullptr) {
        self->progress = progress;
        self->m_uploadProgressCallback(self->progress);
    }
    return 0;
}

void NetworkMachine::uploadHTTP(const char *filename, const char *uploadAs)
{
    std::string url = "http://" + ip + "/upload.cgi:" + std::to_string(m_httpPort);
    states->uploading = true;
    auto http = Http::post(std::move(url));
    http.form_add_file("file", filename, uploadAs)
        .on_complete([&](std::string body, unsigned status) {states->uploading = false;})
        .on_error([&](std::string body, std::string error, unsigned status) {
            states->uploading = false;
            BOOST_LOG_TRIVIAL(error) << boost::format("%1%: Error uploading file: %2%, HTTP %3%, body: `%4%`") % name % error % status % body;
        })
        .on_progress([&](Http::Progress progress, bool &cancel) {
            xfercb(static_cast<void *>(this), progress.dltotal, progress.dlnow, progress.ultotal, progress.ulnow);
        })
        .perform_sync();
}

void NetworkMachine::uploadFTP(const char *filename, const char *uploadAs)
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (!curl) return;
    fs::path path = fs::path(filename);
    boost::system::error_code ec;
    boost::uintmax_t filesize = file_size(path, ec);
    std::unique_ptr<fs::ifstream> putFile;

    states->uploading = true;
    progress = 0;
    m_uploadProgressCallback(progress); // reset
    if (!ec) {
        putFile = std::make_unique<fs::ifstream>(path, ios_base::in | ios_base::binary);
        ::curl_easy_setopt(curl, CURLOPT_READDATA, (void *) (putFile.get()));
        ::curl_easy_setopt(curl, CURLOPT_INFILESIZE, filesize);
    }

    std::string pFilename = *uploadAs ? uploadAs : path.filename().string();
    char *encodedFilename = ::curl_easy_escape(curl, pFilename.c_str(), pFilename.length());

    std::string url = "ftp://" + ip + ":" + std::to_string(m_ftpPort) + "/" + std::string(encodedFilename);
    ::curl_easy_setopt(curl, CURLOPT_USERNAME, "zaxe");
    ::curl_easy_setopt(curl, CURLOPT_PASSWORD, "zaxe");
    ::curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    ::curl_easy_setopt(curl, CURLOPT_READFUNCTION, file_read_cb);
    ::curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    ::curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    ::curl_easy_setopt(curl, CURLOPT_VERBOSE, get_logging_level() >= 5);
    ::curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfercb);
    ::curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, static_cast<void *>(this));
    ::curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0L);

    if ( ! attr->isNoneTLS) {
        ::curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_CONTROL);
        ::curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        ::curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        ::curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
#ifndef __WINDOWS__
        ::curl_easy_setopt(curl, CURLOPT_SSL_CIPHER_LIST, "AES256-GCM-SHA384");
#endif
    }
    res = curl_easy_perform(curl);
    ::curl_free(encodedFilename);
    ::curl_easy_cleanup(curl);
    putFile.reset();
    states->uploading = false;
    if (CURLE_OK != res) {
        BOOST_LOG_TRIVIAL(warning) << boost::format("Networkmachine - Couldn't connect to machine [%1% - %2%] for uploading print. ERROR_CODE: %3%") % name % ip % res;
        return;
    }

    curl_global_cleanup();
}

void NetworkMachine::upload(const char *filename, const char *uploadAs)
{
    if (attr->isHttp) {
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
