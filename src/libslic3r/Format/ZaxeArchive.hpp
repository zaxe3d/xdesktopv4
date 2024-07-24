#pragma once

#include "libslic3r/Zipper.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

#include "nlohmann/json.hpp"

namespace Slic3r {
class ZaxeArchive
{
public:
    ZaxeArchive(const std::string& tmp_dir);

    std::string get_info(const std::string& key, int plate_idx = -1 /* first plate */) const;
    std::string get_path() const;

    void reset();
    void append(const ThumbnailsList& thumbnails,
                const Print&          print,
                const std::string&    temp_gcode_output_path,
                const std::string&    model_path);
    void prepare_file();

protected:
    std::string             tmp_dir;
    nlohmann::json          info;
    std::string             path;
    std::shared_ptr<Zipper> zipper{nullptr};

    void _append(const ThumbnailsList& thumbnails,
                 const Print&          print,
                 const std::string&    temp_gcode_output_path,
                 const std::string&    model_path);
};

} // namespace Slic3r
