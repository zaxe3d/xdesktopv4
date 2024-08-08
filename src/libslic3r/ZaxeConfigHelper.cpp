#include "ZaxeConfigHelper.hpp"

namespace Slic3r {

std::string ZaxeConfigHelper::get_cfg_value(const DynamicPrintConfig& cfg,
                                            const std::string&        key,
                                            const std::string&        default_val,
                                            const bool                only_first_occurence,
                                            const char                delimeter)
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

std::pair<std::string, std::string> ZaxeConfigHelper::get_printer_model(const DynamicPrintConfig& cfg, bool uppercase)
{
    std::string dM = get_cfg_value(cfg, "printer_model");
    if (uppercase) {
        dM = boost::to_upper_copy(dM);
    }
    std::string printer_sub_model{};
    size_t      dM_pos = dM.find(' ');
    if (dM_pos != std::string::npos) {
        printer_sub_model = dM.substr(dM_pos + 1);
        dM                = dM.substr(0, dM_pos);
    }
    boost::replace_all(dM, "+", "PLUS");

    return {dM, printer_sub_model};
}

std::string ZaxeConfigHelper::get_nozzle(const DynamicPrintConfig& cfg, bool uppercase)
{
    auto [dM, printer_sub_model] = get_printer_model(cfg, uppercase);

    std::string nozzle{};
    if (!printer_sub_model.empty()) {
        nozzle.append(printer_sub_model).append(" ");
    }
    nozzle.append(get_cfg_value(cfg, "printer_variant", "0.4", true));

    return nozzle;
}

std::string ZaxeConfigHelper::get_material(const DynamicPrintConfig& cfg)
{
    return get_cfg_value(cfg, "filament_notes", "0", true, ';'); // FIXME change this to filament code later.
}
} // namespace Slic3r