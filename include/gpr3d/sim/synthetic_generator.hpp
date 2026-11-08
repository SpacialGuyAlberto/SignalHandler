#pragma once

#include "gpr3d/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gpr3d::sim {

/// Cylindrical buried utility / conduit (e.g., fiber-optic duct, water/gas pipe) creating a diffraction hyperbola.
struct BuriedTarget {
    double position_along_track_m{2.5};  ///< Horizontal distance along survey line (m)
    double depth_m{0.80};                ///< Depth of top of conduit below surface (m)
    double radius_m{0.05};               ///< Pipe radius in meters
    double reflectivity{-0.85};          ///< Reflection coefficient [-1, 1]
};

/// Planar or gently dipping stratigraphic soil/pavement layer interface.
struct LayerHorizon {
    double base_depth_m{0.30};           ///< Depth at x = 0 m
    double slope_m_per_m{0.01};          ///< Dip slope dz/dx along survey line
    double reflectivity{0.35};           ///< Interface reflection coefficient
};

struct SyntheticSurveyConfig {
    std::size_t num_traces{250};
    std::size_t samples_per_trace{512};
    double sample_interval_ns{0.08};     ///< 0.08 ns -> 12.5 GHz sampling, 40.96 ns window (~2.0 m depth)
    double trace_spacing_m{0.04};        ///< 4 cm step -> 10.0 m profile length
    double scans_per_second{50.0};
    double antenna_freq_mhz{400.0};
    double relative_permittivity{9.0};   ///< eps_r = 9 -> v ~ 0.10 m/ns
    double time_zero_ns{3.2};            ///< Direct wave offset in raw signal

    bool include_direct_wave{true};      ///< Simulate strong surface coupling wave
    bool include_wow_drift{true};        ///< Simulate low-frequency inductive wow bias
    double attenuation_np_per_ns{0.035}; ///< Realistic medium loss factor
    double noise_std{0.015};             ///< Additive white Gaussian noise level
    std::uint32_t random_seed{42};

    Wgs84Coord start_wgs84{
        .latitude_deg = 52.520008,       ///< Default: Berlin urban fiber route
        .longitude_deg = 13.404954,
        .altitude_m = 34.50
    };
    double azimuth_deg{72.0};            ///< Survey street heading (deg clockwise from North)
    double curvature_deg_per_m{0.4};     ///< Slight realistic street curve

    std::vector<BuriedTarget> targets{
        {.position_along_track_m = 2.50, .depth_m = 0.65, .radius_m = 0.04, .reflectivity = 0.90},
        {.position_along_track_m = 5.20, .depth_m = 1.15, .radius_m = 0.08, .reflectivity = -0.95},
        {.position_along_track_m = 7.80, .depth_m = 0.85, .radius_m = 0.05, .reflectivity = 0.80}
    };

    std::vector<LayerHorizon> horizons{
        {.base_depth_m = 0.25, .slope_m_per_m = 0.005, .reflectivity = 0.30},
        {.base_depth_m = 1.45, .slope_m_per_m = -0.012, .reflectivity = 0.40}
    };
};

class SyntheticGenerator {
public:
    /// Evaluate a normalized Ricker (Mexican hat) radar wavelet centered at t = 0 ns.
    [[nodiscard]] static double ricker_wavelet(double t_ns, double center_freq_mhz) noexcept;

    /// Generate a realistic georeferenced raw GPR B-scan with buried conduits, layers, clutter, and GPS.
    [[nodiscard]] static BScan generate_bscan(const SyntheticSurveyConfig& cfg = {});

    /// Generate a parallel 3D grid of B-scan lines crossing 3D linear utilities/conduits.
    [[nodiscard]] static std::vector<BScan> generate_3d_grid(
        std::size_t num_parallel_lines = 5,
        double line_spacing_m = 0.50,
        const SyntheticSurveyConfig& base_cfg = {});
};

}  // namespace gpr3d::sim

