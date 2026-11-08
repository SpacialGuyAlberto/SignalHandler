#pragma once

#include "gpr3d/core/types.hpp"
#include "gpr3d/dsp/interpreter.hpp"
#include "gpr3d/dsp/signal_processor.hpp"
#include "gpr3d/scene/objects_3d.hpp"

#include <span>

namespace gpr3d::dsp {

struct ObjectTransformConfig {
    bool preprocess_signals{true};               ///< Run dual-branch GPR conditioning + Kirchhoff migration
    double migration_aperture_m{1.5};            ///< Kirchhoff aperture (m) for focusing conduit hyperbolas
    double min_depth_m{0.10};                    ///< Minimum subsurface depth (m) for object extraction
    double max_depth_m{5.00};                    ///< Maximum subsurface depth (m)

    bool extract_horizons{true};                 ///< Track continuous stratigraphic layers
    double horizon_min_coverage{0.65};           ///< Minimum fraction of traces where horizon is coherent
    double horizon_threshold{0.20};              ///< Relative envelope threshold for horizon tracking

    bool extract_conduits{true};                 ///< Reconstruct 3D cylindrical conduits / pipes
    double conduit_threshold{0.42};              ///< Normalized migrated envelope threshold for conduits
    double single_line_half_length_m{0.60};      ///< Cross-track extrusion half-length (m) on single B-scan
    int tube_radial_segments{16};                ///< Cylinder mesh tessellation

    bool extract_anomalies{true};                ///< Cluster remaining 3D reflection blobs into anomalies
    double anomaly_threshold{0.32};              ///< Envelope threshold for localized anomalies

    double point_cloud_threshold{0.22};          ///< Threshold for accompanying 3D point cloud
    Wgs84Coord fallback_origin_wgs84{
        .latitude_deg = 52.520008,
        .longitude_deg = 13.404954,
        .altitude_m = 34.50
    };
    double fallback_azimuth_deg{90.0};
};

/// Transforms single or multi-line GPR B-scans into semantic 3D meshes (conduits, stratigraphic
/// horizons, anomalies, terrain surface) and computes waveform/spectral fluctuation profiles.
class SignalObjectTransformer {
public:
    /// Transform a single B-scan into a complete georeferenced 3D scene and fluctuation report.
    [[nodiscard]] static scene::Scene3D transform(
        const BScan& bscan,
        const ObjectTransformConfig& cfg = {});

    /// Transform a multi-line 3D GPR survey grid into a unified 3D scene, automatically linking
    /// conduits and triangulating stratigraphic horizon surfaces across parallel lines.
    [[nodiscard]] static scene::Scene3D transform_grid(
        std::span<const BScan> bscans,
        const ObjectTransformConfig& cfg = {});

    /// Compute detailed waveform, Hilbert phase/frequency, and FFT spectral fluctuations on a B-scan.
    [[nodiscard]] static scene::SignalFluctuationSummary analyze_fluctuations(
        const BScan& processed_bscan);

    /// Mesh generation helpers exposed for custom 3D scene assembly and testing.
    [[nodiscard]] static scene::Mesh3D build_tube_mesh(
        const std::string& name,
        std::span<const EnuCoord> centerline,
        double radius_m,
        int radial_segments,
        const scene::ColorRgba& color,
        float metallic = 0.2f,
        float roughness = 0.5f);

    [[nodiscard]] static scene::Mesh3D build_ellipsoid_mesh(
        const std::string& name,
        const EnuCoord& center,
        const scene::Vec3& radii_m,
        int lat_rings,
        int lon_segments,
        const scene::ColorRgba& color);
};

}  // namespace gpr3d::dsp

