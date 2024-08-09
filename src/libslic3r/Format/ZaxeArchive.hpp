#pragma once

#include "libslic3r/Zipper.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

#include "nlohmann/json.hpp"

namespace Slic3r {
class ZaxeArchive
{
public:
    ZaxeArchive(const std::string& path, bool is_multi_plate);

    std::string get_info(const std::string& key, int plate_idx = -1 /* first plate */) const;
    std::string get_path() const;

    void append(const ThumbnailsList& thumbnails,
                const Print&          print,
                const std::string&    temp_gcode_output_path,
                const std::string&    model_path);
    void prepare_file();
    bool support_multiplate() const { return is_multi_plate; }

protected:
    std::string             path;
    bool                    is_multi_plate;
    nlohmann::json          info;
    std::shared_ptr<Zipper> zipper{nullptr};

    void _append(const ThumbnailsList& thumbnails,
                 const Print&          print,
                 const std::string&    temp_gcode_output_path,
                 const std::string&    model_path);
};

} // namespace Slic3r
