#ifndef ZAXEARCHIVE_H_
#define ZAXEARCHIVE_H_

#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/hex.hpp>
#include <stdio.h>

#include "libslic3r/Zipper.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <openssl/md5.h> // for md5 checksum.
#include <fstream>
#include <sstream>

#include "libslic3r/Time.hpp"

#include "libslic3r/miniz_extension.hpp"
#include "libslic3r/LocalesUtils.hpp"
#include "libslic3r/GCode/ThumbnailData.hpp"

using namespace boost::algorithm;
using namespace boost::property_tree;
using namespace std;
using ConfMap = std::map<std::string, std::string>;

namespace Slic3r {
class ZaxeArchive {
public:
    ZaxeArchive() = default;
    /// Actually perform the export.
    void export_print(const string archive_path, ThumbnailsList thumbnails, const Print &print, const string temp_gcode_output_path, const bool bed_level);
    std::string get_info(const std::string &key) const;
    std::string get_info_list_str() const;
protected:
    void generate_info_file(ConfMap &m, const Print &print);
    ConfMap m_infoconf;
};

} // namespace Slic3r
#endif // ZAXEARCHIVE_H_
