#pragma once

#include "gpr3d/core/types.hpp"

#include <filesystem>
#include <vector>

namespace gpr3d::io {

/// Read GNSS coordinate fixes from CSV, MALA .cor, or NMEA-0183 ($GPGGA / $GNGGA) files.
[[nodiscard]] std::vector<GpsFix> read_gps_file(const std::filesystem::path& path);

/// Parse a single NMEA-0183 $GPGGA or $GNGGA sentence into a GpsFix (returns nullopt if invalid/non-GGA).
[[nodiscard]] std::optional<GpsFix> parse_nmea_gga(const std::string& sentence);

}  // namespace gpr3d::io

