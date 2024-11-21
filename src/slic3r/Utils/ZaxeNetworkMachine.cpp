#include "ZaxeNetworkMachine.hpp"

#include "nlohmann/json.hpp"
#include "libslic3r/Utils.hpp"

namespace Slic3r {

wxDEFINE_EVENT(EVT_MACHINE_OPEN, wxCommandEvent);
wxDEFINE_EVENT(EVT_MACHINE_CLOSE, wxCommandEvent);
wxDEFINE_EVENT(EVT_MACHINE_UPDATE, wxCommandEvent);
wxDEFINE_EVENT(EVT_MACHINE_SWITCH, wxCommandEvent);
wxDEFINE_EVENT(EVT_MACHINE_AVATAR_READY, wxCommandEvent);
wxDEFINE_EVENT(EVT_MACHINE_REGISTER, wxCommandEvent);

ZaxeNetworkMachine::ZaxeNetworkMachine() : states{std::make_shared<MachineStates>()}, attr{std::make_shared<MachineAttributes>()} {}

// todo zaxe
bool ZaxeNetworkMachine::hasRemoteUpdate() const
{
    return is_there(attr->device_model, {"z3"}) && attr->firmware_version >= Semver(3, 5, 70);
}

bool ZaxeNetworkMachine::canToggleLeds() const
{
    return is_there(attr->device_model, {"z3", "z4", "x4"}) && attr->firmware_version >= Semver(3, 5, 70);
}

bool ZaxeNetworkMachine::hasStl() const { return is_there(attr->device_model, {"z2", "z3", "z4", "x4"}); }

bool ZaxeNetworkMachine::hasThumbnails() const { return is_there(attr->device_model, {"z1", "z2", "z3", "z4", "x4"}); }

bool ZaxeNetworkMachine::hasCam() const { return is_there(attr->device_model, {"z2", "z3", "z4", "x4"}); }

bool ZaxeNetworkMachine::hasSnapshot() const { return is_there(attr->device_model, {"z1", "z2", "z3", "z4", "x4"}); }

bool ZaxeNetworkMachine::canUnloadFilament() const { return is_there(attr->device_model, {"z1", "z2", "z3", "z4", "x4"}); }

bool ZaxeNetworkMachine::canPrintMultiPlate() const
{
    return is_there(attr->device_model, {"z3", "z4", "x4"}) && attr->firmware_version >= Semver(3, 5, 78);
}

bool ZaxeNetworkMachine::hasPrinterCover() const { return is_there(attr->device_model, {"z1", "z3", "x1", "x2", "x3"}); };

bool ZaxeNetworkMachine::hasFilamentPresentInfo() const { return attr->firmware_version >= Semver(3, 5, 0); }

void ZaxeNetworkMachine::handle_device_message(const std::string& message)
{
    try {
        auto j = nlohmann::json::parse(message);

        std::string event{};
        j.at("event").get_to(event);

        if (event == "ping" || event == "temperature_change") {
            return;
        }

        auto get_material_label = [&]() {
            try {
                if (j.contains("material_label")) {
                    return j.value("material_label", "Zaxe ABS");
                }
            } catch (...) {}

            std::map<std::string, std::string> material_map = {{"zaxe_abs", "Zaxe ABS"},
                                                               {"zaxe_pla", "Zaxe PLA"},
                                                               {"zaxe_flex", "Zaxe FLEX"},
                                                               {"zaxe_petg", "Zaxe PETG"},
                                                               {"custom", "Custom"}};

            auto material = boost::to_lower_copy(j.value("material", "zaxe_abs"));

            if (auto iter = material_map.find(material); iter != material_map.end())
                return iter->second;
            return material;
        };

        auto get_bool = [](const auto& j, const std::string& key) {
            std::string val = j.value(key, "False");
            return val == "True";
        };

        if (event == "hello") {
            name                 = j.value("name", "Unknown");
            attr->serial_no      = j.value("serial_no", "Unknown");
            attr->device_model   = boost::to_lower_copy(j.value("device_model", "x1"));
            attr->material       = boost::to_lower_copy(j.value("material", "zaxe_abs"));
            attr->material_label = get_material_label();
            attr->nozzle         = j.value("nozzle", "0.4");
            attr->is_lite        = is_there(attr->device_model, {"lite", "x3"});
            attr->is_http        = j.value("protocol", "") == "http";
            attr->is_none_TLS    = is_there(attr->device_model, {"z2", "z3", "z4", "x4"}) || attr->is_lite;
            // printing
            attr->printing_file  = j.value("filename", "");
            attr->elapsed_time   = j.value("elapsed_time", 0);
            attr->estimated_time = j.value("estimated_time", "");
            attr->start_time     = wxDateTime::Now().GetTicks() - attr->elapsed_time;
            if (!attr->is_lite) {
                attr->has_pin            = get_bool(j, "has_pin");
                attr->has_nfc_spool      = get_bool(j, "has_nfc_spool");
                attr->filament_color     = get_bool(j, "filament_color");
                attr->remaining_filament = j.value("filament_remaining", 0);
            }
            std::vector<std::string> fwV;
            split(fwV, boost::to_lower_copy(j.value("version", "1.0.0")), boost::is_any_of("."));
            attr->firmware_version = Semver(stoi(fwV[0]), stoi(fwV[1]), stoi(fwV[2]));
        }

        if (event == "hello" || event == "states_update") {
            states->calibrating      = get_bool(j, "is_calibrating");
            states->bed_occupied     = get_bool(j, "is_bed_occupied");
            states->bed_dirty        = get_bool(j, "is_bed_dirty");
            states->usb_present      = get_bool(j, "is_usb_present");
            states->preheating       = get_bool(j, "is_preheat");
            states->printing         = get_bool(j, "is_printing");
            states->heating          = get_bool(j, "is_heating");
            states->paused           = get_bool(j, "is_paused");
            states->has_error        = get_bool(j, "is_error");
            states->leds_switched_on = get_bool(j, "is_leds");
            states->updating_fw      = get_bool(j, "is_downloading");
            states->filament_present = hasFilamentPresentInfo() ? get_bool(j, "is_filament_present") : true;
        }

        if (event == "print_progress" || event == "temperature_progress" || event == "calibration_progress") {
            progress                    = j.value("progress", 0.f);
            states->uploading_zaxe_file = false;
        } else if (event == "upload_progress") {
            progress                    = j.value("progress", 0.f);
            states->uploading_zaxe_file = true;
        }else  if (event == "upload_done") {
            states->uploading_zaxe_file = false;
        }

        if (event == "new_name") {
            name = j.value("name", "Zaxe");
        } else if (event == "material_change") {
            attr->material       = boost::to_lower_copy(j.value("material", "zaxe_abs"));
            attr->material_label = get_material_label();
        } else if (event == "nozzle_change") {
            attr->nozzle = j.value("nozzle", "0.4");
        } else if (event == "pin_change") {
            attr->has_pin = boost::to_lower_copy(j.value("has_pin", "false")) == "true";
        } else if (event == "start_print") {
            attr->printing_file  = j.value("filename", "");
            attr->elapsed_time   = j.value("elapsed_time", 0);
            attr->start_time     = wxDateTime::Now().GetTicks() - attr->elapsed_time;
            attr->estimated_time = j.value("estimated_time", "");
        } else if (event == "spool_data_change") {
            attr->has_nfc_spool  = boost::to_lower_copy(j.value("has_nfc_spool", "false")) == "true";
            attr->filament_color = boost::to_lower_copy(j.value("filament_color", "unknown"));
        } else if (event == "temperature_update") {
            attr->nozzle_temp        = j.value("ext_temp", 0.f);
            attr->target_nozzle_temp = j.value("ext_temp_set", 0.f);
            attr->bed_temp           = j.value("bed_temp", 0.f);
            attr->target_bed_temp    = j.value("bed_temp_set", 0.f);
        }

        if (event == "hello") {
            hello_received = true;
            auto evt       = new wxCommandEvent(EVT_MACHINE_OPEN);
            wxQueueEvent(this, evt);
        } else {
            auto evt = new wxCommandEvent(EVT_MACHINE_UPDATE);
            evt->SetString(event);
            wxQueueEvent(this, evt);
        }

    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(warning) << __func__ << " - Name: " << name << " serial: " << attr->serial_no << " error: " << e.what()
                                   << " - content: " << message;
    }
}

} // namespace Slic3r