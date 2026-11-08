#pragma once

#include "gpr3d/core/types.hpp"

#include <filesystem>
#include <memory>
#include <optional>

namespace gpr3d::io {

struct ReadOptions {
    std::optional<int> utm_zone;              ///< Optional UTM zone if radar file stores metric X/Y coords
    bool utm_northern{true};                  ///< Hemisphere when `utm_zone` is used
    std::optional<double> override_epsr;      ///< Override medium relative permittivity eps_r
    std::optional<double> override_dt_ns;     ///< Override sample interval (ns) if header is non-standard
};

/// Abstract interface for reading GPR radargrams into a unified BScan representation.
class RadarReader {
public:
    virtual ~RadarReader() = default;
    [[nodiscard]] virtual BScan read(
        const std::filesystem::path& path,
        const ReadOptions& options = {}) const = 0;
};

/// Automatically select SEG-Y (.sgy/.segy), GSSI (.dzt), or MALA (.rd3/.rad) reader and load BScan.
/// If a companion GPS file (<stem>.cor, <stem>.gps, <stem>.nmea, <stem>.csv) exists next to the
/// radar file and traces are not yet georeferenced, it is loaded and synchronized automatically.
[[nodiscard]] BScan read_radar_file(
    const std::filesystem::path& path,
    const ReadOptions& options = {});

}  // namespace gpr3d::io

