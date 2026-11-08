#pragma once

#include "gpr3d/io/exporter_3d.hpp"
#include "gpr3d/scene/objects_3d.hpp"

#include <filesystem>

namespace gpr3d::io {

struct SceneExportOptions {
    CoordFrame3D obj_coord_frame{CoordFrame3D::LocalENU};  ///< LocalENU avoids float jitter in 3D viewers; UTM available for CAD/GIS
    bool include_ground_surface{true};
};

/// Exports reconstructed semantic 3D GPR objects (conduits, stratigraphic horizons, anomalies)
/// to Wavefront OBJ+MTL, Khronos glTF 2.0 Binary (.glb), and 3D GeoJSON (.geojson).
class SceneExporter3D {
public:
    /// Write Wavefront .obj and companion .mtl material library.
    static void write_obj(
        const std::filesystem::path& obj_path,
        const scene::Scene3D& scene,
        const SceneExportOptions& opts = {});

    /// Write self-contained Khronos glTF 2.0 Binary (.glb) with PBR materials and geospatial extras.
    static void write_glb(
        const std::filesystem::path& glb_path,
        const scene::Scene3D& scene,
        const SceneExportOptions& opts = {});

    /// Write RFC 7946 3D GeoJSON FeatureCollection with WGS84 [lon, lat, alt_m] geometries.
    static void write_geojson(
        const std::filesystem::path& geojson_path,
        const scene::Scene3D& scene);

    /// Dispatch automatically based on file extension (.obj, .glb, .geojson/.json).
    static void write_auto(
        const std::filesystem::path& path,
        const scene::Scene3D& scene,
        const SceneExportOptions& opts = {});
};

}  // namespace gpr3d::io

