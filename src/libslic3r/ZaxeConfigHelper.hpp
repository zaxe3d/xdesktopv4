#pragma once

#include "PrintConfig.hpp"

namespace Slic3r {
class ZaxeConfigHelper
{
public:
    static std::string get_cfg_value(const DynamicPrintConfig& cfg,
                                     const std::string&        key,
                                     const std::string&        default_val          = "",
                                     const bool                only_first_occurence = false,
                                     const char                delimeter            = ',');

    static std::pair<std::string, std::string> get_printer_model(const DynamicPrintConfig& cfg);
    static std::string                         get_nozzle(const DynamicPrintConfig& cfg);
    static std::string                         get_material(const DynamicPrintConfig& cfg);
};
} // namespace Slic3r