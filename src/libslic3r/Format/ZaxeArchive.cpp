#include "ZaxeArchive.hpp"

#include "libslic3r/Utils.hpp"
#include "libslic3r/Time.hpp"
#include "libslic3r/miniz_extension.hpp"
#include "libslic3r/LocalesUtils.hpp"

#include <openssl/md5.h>

#define ZAXE_FILE_VERSION "4.0.0"

namespace Slic3r {

namespace {

std::string get_cfg_value(const DynamicPrintConfig& cfg,
                          const std::string&        key,
                          const std::string&        default_val          = "",
                          const bool                only_first_occurence = false,
                          const char                delimeter            = ',')
{
    std::string ret = default_val; // start with default.
    if (cfg.has(key)) {
        auto opt = cfg.option(key);
        if (opt)
            ret = opt->serialize();
        if (ret.empty())
            ret = default_val; // again to default when empty.
        if (only_first_occurence) {
            const auto colon_idx = ret.find_first_of(delimeter);
            if (std::string::npos != colon_idx) {
                return ret.substr(0, colon_idx); // return first occurence.
            }
        }
    }
    return ret;
}

std::string generate_md5_checksum(const std::string& file_path)
{
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    char          buffer[4096];
    std::ifstream file(file_path.c_str(), std::ifstream::binary);

    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        MD5_Update(&md5Context, buffer, file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    std::stringstream md5string;
    md5string << std::hex << std::uppercase << std::setfill('0');
    for (const auto& byte : result)
        md5string << std::setw(2) << (int) byte;

    return boost::to_lower_copy(md5string.str());
}

std::ifstream::pos_type read_binary_into_buffer(const char* path, std::vector<char>& bytes)
{
    std::ifstream           ifs(path, std::ios::in | std::ios::binary | std::ios::ate);
    std::ifstream::pos_type fileSize = ifs.tellg(); // get the file size.
    ifs.seekg(0, std::ios::beg);                    // seek to beginning.
    bytes.resize(fileSize);                         // resize.
    ifs.read(bytes.data(), fileSize);               // read.
    return fileSize;
}

static void write_thumbnail(std::shared_ptr<Zipper> zipper, const ThumbnailData& data, const std::string& file_name)
{
    if (!zipper) {
        return;
    }
    size_t png_size = 0;

    void* png_data = tdefl_write_image_to_png_file_in_memory_ex((const void*) data.pixels.data(), data.width, data.height, 4, &png_size,
                                                                MZ_DEFAULT_LEVEL, 1);

    if (png_data != nullptr) {
        zipper->add_entry(file_name, static_cast<const std::uint8_t*>(png_data), png_size);

        mz_free(png_data);
    }
}
} // namespace

std::string ZaxeArchive::get_info(const std::string& key, int plate_idx) const
{
    try {
        const auto& plates = info["plates"];

        for (const auto& plate : plates) {
            if (plate_idx == -1 || plate["plate_idx"] == plate_idx) {
                if (plate.contains(key) && plate[key].is_string()) {
                    return plate[key].get<std::string>();
                }
                break;
            }
        }
    } catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "ZaxeArchive get_info failed: " << e.what();
    }
    return "";
}

ZaxeArchive::ZaxeArchive(const std::string& tmp_dir) : tmp_dir(tmp_dir)
{
    info           = nlohmann::json::object();
    info["plates"] = nlohmann::json::array();
}

void ZaxeArchive::reset()
{
    //BOOST_LOG_TRIVIAL(error) << "ZaxeArchive::reset";
    info           = nlohmann::json::object();
    info["plates"] = nlohmann::json::array();
    zipper.reset();

    auto get_date_time = []() {
        std::time_t        now = std::time(nullptr);
        std::tm*           lt  = std::localtime(&now);
        std::ostringstream ss;
        ss << std::put_time(lt, "%Y%m%d_%H%M%S");
        return ss.str();
    };

    boost::filesystem::path temp_path(tmp_dir);
    boost::filesystem::path zip_path(temp_path);
    zip_path /= (boost::format("xdesktop_%1%.zaxe") % get_date_time()).str();
    path   = zip_path.string();
    zipper = std::make_shared<Zipper>(path);
}

std::string ZaxeArchive::get_path() const { return path; }

void ZaxeArchive::append(const ThumbnailsList& thumbnails,
                         const Print&          print,
                         const std::string&    temp_gcode_output_path,
                         const std::string&    model_path)
{
    try {
        _append(thumbnails, print, temp_gcode_output_path, model_path);
    } catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Appending print info to zaxe archive failed: " << e.what();
    }
}

void ZaxeArchive::_append(const ThumbnailsList& thumbnails,
                          const Print&          print,
                          const std::string&    temp_gcode_output_path,
                          const std::string&    model_path)
{
    int plate_idx = print.get_plate_index();

    auto& cfg = print.full_print_config();

    nlohmann::json j;
    j["checksum"]   = generate_md5_checksum(temp_gcode_output_path);
    j["bed_level"]  = cfg.option<ConfigOptionBool>("zaxe_bed_leveling")->value ? "on" : "off";
    j["arc_welder"] = "off"; // backward compatibility

    std::string fill_density = get_cfg_value(cfg, "sparse_infill_density");
    fill_density.replace(fill_density.find("%"), 1, "");

    bool has_raft  = stoi(get_cfg_value(cfg, "raft_layers")) > 0;
    bool has_skirt = stoi(get_cfg_value(cfg, "skirt_loops")) > 0;

    std::string dM = boost::to_upper_copy(get_cfg_value(cfg, "printer_model"));
    std::string printer_sub_model{};
    size_t      dM_pos = dM.find(' ');
    if (dM_pos != std::string::npos) {
        printer_sub_model = dM.substr(dM_pos + 1);
        dM                = dM.substr(0, dM_pos);
    }
    boost::replace_all(dM, "+", "PLUS");

    std::string nozzle{};
    if (!printer_sub_model.empty()) {
        nozzle.append(printer_sub_model).append(" ");
    }
    nozzle.append(get_cfg_value(cfg, "printer_variant", "0.4", true));

    PrintStatistics stats = print.print_statistics();

    j["name"]                 = print.output_filename();
    j["version"]              = ZAXE_FILE_VERSION;
    j["plate_idx"]            = plate_idx;
    j["duration"]             = get_time_hms(stats.estimated_normal_print_time);
    j["raft"]                 = has_raft ? "raft" : (has_skirt ? "skirt" : "none");
    j["layer_height"]         = cfg.opt_float("layer_height");
    j["infill_density"]       = stoi(fill_density);
    j["support_angle"]        = get_cfg_value(cfg, "support_angle");
    j["material"]             = get_cfg_value(cfg, "filament_notes", "0", true, ';'); // FIXME change this to filament code later.
    j["model"]                = dM;
    j["sub_model"]            = printer_sub_model;
    j["printer_profile"]      = get_cfg_value(cfg, "printer_settings_id");
    j["filament_used"]        = stats.total_used_filament;
    j["nozzle_diameter"]      = nozzle;
    j["extruder_temperature"] = cfg.opt_int("nozzle_temperature_initial_layer", 0);
    j["bed_temperature"]     = print.config().option<ConfigOptionInts>(get_bed_temp_1st_layer_key(print.config().curr_bed_type))->get_at(0);
    j["standby_temperature"] = cfg.opt_int("nozzle_temperature", 0) + cfg.opt_int("standby_temperature_delta");

    std::vector<char> bytes;
    std::string       gcode;
    load_string_file(temp_gcode_output_path, gcode);

    auto zaxe_code_name = boost::str(boost::format("data_%d.zaxe_code") % plate_idx);
    j["zaxe_code"]      = zaxe_code_name;

    j["snapshots"] = nlohmann::json::array();

    if (zipper) {
        zipper->add_entry(zaxe_code_name);
        *zipper << gcode;

        int snapshot_idx = 0;
        for (const ThumbnailData& data : thumbnails) {
            if (data.is_valid()) {
                auto snaphot_name = boost::str(boost::format("snapshot_%d_%d.png") % plate_idx % snapshot_idx++);
                write_thumbnail(zipper, data, snaphot_name);
                j["snapshots"].push_back(snaphot_name);
            }
        }
    }

    boost::filesystem::path temp_path(temp_gcode_output_path);
    if (zipper && !model_path.empty() && is_there(dM, {"Z2", "Z3"})) {
        std::vector<char>       bytes;
        std::ifstream::pos_type file_size = read_binary_into_buffer(model_path.c_str(), bytes);
        auto                    stl_name  = boost::str(boost::format("model_%d.stl") % plate_idx);
        zipper->add_entry(stl_name, bytes.data(), file_size);
        j["stl"] = stl_name;
    }

    info["plates"].push_back(j);
}

void ZaxeArchive::prepare_file()
{
    BOOST_LOG_TRIVIAL(info) << "Preparing Zaxe file...";

    if (!zipper) {
        return;
    }

    info["name"]             = boost::filesystem::path(zipper->get_filename()).stem().string();
    info["slicer_version"]   = SLIC3R_VERSION;
    info["xdesktop_version"] = SoftFever_VERSION;

    zipper->add_entry("info.json");
    *zipper << info.dump(4);

    zipper->finalize();
    BOOST_LOG_TRIVIAL(info) << "Zaxe file generated successfully to: " << zipper->get_filename();
}

} // namespace Slic3r
