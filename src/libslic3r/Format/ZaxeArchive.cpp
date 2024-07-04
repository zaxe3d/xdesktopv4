#include "ZaxeArchive.hpp"

#include "libslic3r/Utils.hpp"

#define ZAXE_FILE_VERSION "3.0.1"

namespace Slic3r {

namespace {
std::string to_json(const ConfMap &m)
{
    ptree pt; // construct root obj.
    stringstream ss;
    // replace string numbers with numbers. better than translator solution imho.
    std::regex reg("\\\"(-?[0-9]+\\.{0,1}[0-9]*)\\\"");

    for (auto &param : m) pt.put(param.first, param.second);

    json_parser::write_json(ss, pt);

    return std::regex_replace(ss.str(), reg, "$1");
}

std::string get_cfg_value(const DynamicPrintConfig &cfg, const std::string &key, const std::string &default_val = "", const bool only_first_occurence = false, const char delimeter = ',')
{
    std::string ret = default_val; // start with default.
    if (cfg.has(key)) {
        auto opt = cfg.option(key);
        if (opt) ret = opt->serialize();
        if (ret.empty()) ret = default_val; // again to default when empty.
        if (only_first_occurence) {
            const auto colon_idx = ret.find_first_of(delimeter);
            if (std::string::npos != colon_idx) {
                return ret.substr(0, colon_idx); // return first occurence.
            }
        }
    }
    return ret;
}

std::string generate_md5_checksum(const string file_path)
{
    MD5_CTX md5Context;
    MD5_Init(&md5Context);
    char buffer[4096];
    ifstream file(file_path.c_str(), ifstream::binary);

    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        MD5_Update(&md5Context, buffer, file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    stringstream md5string;
    md5string << hex << uppercase << setfill('0');
    for (const auto &byte: result)
        md5string << setw(2) << (int)byte;

    return to_lower_copy(md5string.str());
}

ifstream::pos_type read_binary_into_buffer(const char* path, vector<char>& bytes)
{
    ifstream ifs(path, ios::in | ios::binary | ios::ate);
    ifstream::pos_type fileSize = ifs.tellg(); // get the file size.
    ifs.seekg(0, ios::beg); // seek to beginning.
    bytes.resize(fileSize); // resize.
    ifs.read(bytes.data(), fileSize); // read.
    return fileSize;
}

static void write_thumbnail(Zipper &zipper, const ThumbnailData &data)
{
    size_t png_size = 0;

    void *png_data = tdefl_write_image_to_png_file_in_memory_ex(
         (const void *) data.pixels.data(), data.width, data.height, 4,
         &png_size, MZ_DEFAULT_LEVEL, 1);

    if (png_data != nullptr) {
        zipper.add_entry("snapshot.png", static_cast<const std::uint8_t *>(png_data), png_size);

        mz_free(png_data);
    }
}
} // namespace

std::string ZaxeArchive::get_info(const string &key) const
{
    auto it = m_infoconf.find(key);
    if (it != m_infoconf.end())
        return it->second;
    return "";
}

std::string ZaxeArchive::get_info_list_str() const
{
    std::stringstream ss;
    for (const auto& [key, value] : m_infoconf) {
        ss << key << ": " << value << std::endl;
    }

    return ss.str();
}

void ZaxeArchive::export_print(
    const string archive_path,
    ThumbnailsList thumbnails,
    const Print &print,
    const string temp_gcode_output_path,
    const bool bed_level
)
{
    Zipper zipper{archive_path};
    boost::filesystem::path temp_path(temp_gcode_output_path);
    vector<char> bytes;
    std::string gcode;

    try {
        m_infoconf = ConfMap{
            { "name", boost::filesystem::path(zipper.get_filename()).stem().string() },
            { "checksum", generate_md5_checksum(temp_gcode_output_path) },
            { "bed_level", bed_level ? "on" : "off" },
            { "arc_welder", "off" }, // backward compatibility
        };
        generate_info_file(m_infoconf, print); // generate info.json contents.
        zipper.add_entry("info.json");
        zipper << to_json(m_infoconf);
        // add gcode
        // load file into string again. FIXME maybe a better way to get the string beforehand.
        load_string_file(temp_gcode_output_path, gcode);
        zipper.add_entry("data.zaxe_code");
        zipper << gcode;
        // add model stl
        if (false && is_there(m_infoconf["model"], {"Z2", "Z3"})) { // export stl only if model is Z2 or Z3.
            string model_path = (temp_path.parent_path() / "model.stl").string();
            ifstream::pos_type fileSize = read_binary_into_buffer(model_path.c_str(), bytes);
            zipper.add_entry("model.stl", bytes.data(), fileSize);
        }
        for (const ThumbnailData& data : thumbnails)
            if (data.is_valid()) write_thumbnail(zipper, data);
        zipper.finalize();
        BOOST_LOG_TRIVIAL(info) << "Zaxe file generated successfully to: " << zipper.get_filename();
    } catch(std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
        throw; // Rethrow the exception
    }
}

void ZaxeArchive::generate_info_file(ConfMap &m, const Print &print)
{
    auto &cfg = print.full_print_config();
    PrintStatistics stats = print.print_statistics();

    // standby temp calculation
    float temp = stof(get_cfg_value(cfg, "nozzle_temperature"));
    float standby_temp_delta = stof(get_cfg_value(cfg, "standby_temperature_delta"));
    char standby_temp_char[100];
    sprintf(standby_temp_char, "%.2f", (temp + standby_temp_delta));
    std::string fill_density = get_cfg_value(cfg, "sparse_infill_density");
    bool has_raft = stoi(get_cfg_value(cfg, "raft_layers")) > 0;
    bool has_skirt = stoi(get_cfg_value(cfg, "skirt_loops")) > 0;
    std::string dM = to_upper_copy(get_cfg_value(cfg, "printer_model"));
    std::string printer_sub_model{};
    size_t dM_pos = dM.find(' ');
    if (dM_pos != std::string::npos) {
        printer_sub_model = dM.substr(dM_pos + 1);
        dM = dM.substr(0, dM_pos);
    }
    boost::replace_all(dM, "+", "PLUS");

    m["version"]                = ZAXE_FILE_VERSION;
    m["duration"]               = get_time_hms(stats.estimated_normal_print_time);
    m["raft"]                   = has_raft ? "raft" : (has_skirt ? "skirt" : "none");
    m["layer_height"]           = get_cfg_value(cfg, "layer_height");
    m["infill_density"]         = fill_density.replace(fill_density.find("%"), 1, "");
    m["support_angle"]          = get_cfg_value(cfg, "support_angle");
    m["material"]               = get_cfg_value(cfg, "filament_notes", "0", true, ';'); // FIXME change this to filament code later.
    m["model"]                  = dM;
    m["sub_model"]              = printer_sub_model;
    m["printer_profile"]        = get_cfg_value(cfg, "printer_settings_id");
    m["filament_used"]          = std::to_string(stats.total_used_filament);
    m["nozzle_diameter"]        = get_cfg_value(cfg, "printer_variant", "0.4", true);
    m["extruder_temperature"]   = get_cfg_value(cfg, "nozzle_temperature_initial_layer", "0", true);
    m["bed_temperature"]        = get_cfg_value(cfg, "eng_plate_temp", "0", true); // todo zaxe
    m["standby_temperature"]    = standby_temp_char;
    m["slicer_version"]         = SLIC3R_VERSION;
    m["xdesktop_version"]       = SoftFever_VERSION;
}

} // namespace Slic3r
