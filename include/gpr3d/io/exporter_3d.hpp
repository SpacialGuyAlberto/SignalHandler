#pragma once

#include "gpr3d/core/types.hpp"

#include <filesystem>

namespace gpr3d::io {

enum class CoordFrame3D {
    UTM,       ///< Primary X=Easting (m), Y=Northing (m), Z=Elevation (m) in double precision
    LocalENU   ///< Primary X=East (m), Y=North (m), Z=Up (m) relative to survey origin
};

struct PlyExportOptions {
    bool binary{false};                         ///< False = ASCII PLY, True = binary_little_endian PLY
    CoordFrame3D primary_frame{CoordFrame3D::UTM};
    bool include_geodetic_properties{true};     ///< Include double `latitude` and `longitude` properties
};

/// Exports georeferenced 3D GPR point clouds to PLY, CSV, and XYZ formats.
class Exporter3D {
public:
    /// Export point cloud to Stanford Polygon (.ply) with RGB intensity colormap and CRS metadata.
    static void write_ply(
        const std::filesystem::path& path,
        const PointCloud3D& cloud,
        const PlyExportOptions& opts = {});

    /// Export full georeferenced table (WGS84 + UTM + ENU + radar attributes) to CSV.
    static void write_csv(
        const std::filesystem::path& path,
        const PointCloud3D& cloud);

    /// Export metric 3D point cloud to .xyz (`X Y Z intensity depth_m lat_deg lon_deg`).
    static void write_xyz(
        const std::filesystem::path& path,
        const PointCloud3D& cloud,
        CoordFrame3D frame = CoordFrame3D::UTM);

    /// Dispatch automatically by file extension (.ply, .csv, .xyz).
    static void write_auto(
        const std::filesystem::path& path,
        const PointCloud3D& cloud,
        const PlyExportOptions& ply_opts = {});
};

}  // namespace gpr3d::io

