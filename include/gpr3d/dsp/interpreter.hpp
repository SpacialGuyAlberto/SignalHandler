#pragma once

#include "gpr3d/core/types.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace gpr3d::dsp {

/// Optional piecewise-constant dielectric layer for stratified road/soil depth conversion.
struct DielectricLayer {
    double thickness_m{0.20};           ///< Layer thickness in meters (ignored for bottom half-space)
    double relative_permittivity{9.0};  ///< Relative permittivity eps_r of this layer
};

enum class ExtractionMode {
    ThresholdedVolume,   ///< Export subsurface samples exceeding `min_intensity_threshold`
    ReflectorPeaksOnly,  ///< Export only local vertical Hilbert envelope peaks (horizons/pipes)
    FullCurtain          ///< Export all samples within [min_depth_m, max_depth_m]
};

struct InterpretationConfig {
    ExtractionMode mode{ExtractionMode::ThresholdedVolume};
    double min_intensity_threshold{0.18};        ///< Normalized [0..1] envelope cutoff
    double min_depth_m{0.05};                    ///< Skip residual surface direct-wave zone (m)
    double max_depth_m{10.0};                    ///< Maximum interpreted depth (m)
    std::size_t vertical_stride{1};              ///< Sample decimation factor (>= 1)
    std::size_t trace_stride{1};                 ///< Horizontal trace decimation factor (>= 1)
    std::optional<double> override_epsr;         ///< Optional homogeneous permittivity override
    std::vector<DielectricLayer> layers{};       ///< Optional multi-layer pavement/soil model
    std::optional<int> forced_utm_zone;          ///< Optional fixed UTM zone for all points
    Wgs84Coord fallback_origin_wgs84{            ///< Used if B-scan has no embedded/external GPS
        .latitude_deg = 52.520008,
        .longitude_deg = 13.404954,
        .altitude_m = 35.0
    };
    double fallback_azimuth_deg{90.0};           ///< Default Eastward azimuth when using fallback origin
};

/// Interprets processed GPR B-scans and transforms radar reflections into georeferenced 3D point clouds.
class RadarInterpreter {
public:
    /// Convert two-way travel time (ns) into vertical depth (m) using homogeneous or layered velocity.
    [[nodiscard]] static double twt_to_depth_m(
        double twt_ns,
        double default_epsr,
        double antenna_separation_m = 0.0,
        const std::vector<DielectricLayer>& layers = {}) noexcept;

    /// Transform a single B-scan into a georeferenced 3D point cloud (WGS84 + UTM + Local ENU).
    [[nodiscard]] static PointCloud3D transform_to_3d(
        const BScan& bscan,
        const InterpretationConfig& cfg = {});

    /// Append interpreted 3D points from an additional B-scan into an existing PointCloud3D,
    /// preserving a unified ENU origin and UTM zone across multi-line 3D surveys.
    static void append_bscan_to_3d(
        PointCloud3D& cloud,
        const BScan& bscan,
        const InterpretationConfig& cfg = {});
};

}  // namespace gpr3d::dsp

