#include "gpr3d/io/reader.hpp"
#include "gpr3d/geo/trajectory_sync.hpp"
#include "gpr3d/io/dzt_reader.hpp"
#include "gpr3d/io/gps_reader.hpp"
#include "gpr3d/io/rd3_reader.hpp"
#include "gpr3d/io/segy_reader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>

namespace gpr3d::io {

namespace {

std::string lower_ext(const std::filesystem::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

}  // namespace

BScan read_radar_file(const std::filesystem::path& path, const ReadOptions& options) {
    const std::string ext = lower_ext(path);

    BScan bscan{};
    if (ext == ".sgy" || ext == ".segy") {
        SegyReader reader;
        bscan = reader.read(path, options);
    } else if (ext == ".dzt") {
        DztReader reader;
        bscan = reader.read(path, options);
    } else if (ext == ".rd3" || ext == ".rad") {
        Rd3Reader reader;
        bscan = reader.read(path, options);
    } else {
        throw std::runtime_error(
            "Unsupported radar file extension '" + ext +
            "' (expected .sgy, .segy, .dzt, .rd3, or .rad): " + path.string());
    }

    // Check if traces already carry valid WGS84 coordinates
    const bool already_georef = std::any_of(
        bscan.traces.begin(), bscan.traces.end(),
        [](const AScan& t) { return t.position_wgs84.has_value(); });

    if (!already_georef && !bscan.empty()) {
        static constexpr std::array<const char*, 6> kCompanionExts = {
            ".cor", ".COR", ".gps", ".nmea", ".csv", ".CSV"
        };
        for (const char* cext : kCompanionExts) {
            auto cpath = path;
            cpath.replace_extension(cext);
            if (std::filesystem::exists(cpath)) {
                const auto fixes = read_gps_file(cpath);
                if (!fixes.empty()) {
                    geo::synchronize_trajectory(bscan, fixes);
                    break;
                }
            }
        }
    }

    return bscan;
}

}  // namespace gpr3d::io

