#include "gpr3d/sim/synthetic_generator.hpp"
#include "gpr3d/geo/crs_transform.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace gpr3d::sim {

double SyntheticGenerator::ricker_wavelet(double t_ns, double center_freq_mhz) noexcept {
    // Convert MHz * ns -> dimensionless cycles: 1 MHz * 1 ns = 1e6 * 1e-9 = 1e-3 cycles
    const double fc_ghz = center_freq_mhz * 1e-3;
    const double u = std::numbers::pi * fc_ghz * t_ns;
    const double u2 = u * u;
    if (u2 > 30.0) {
        return 0.0;
    }
    return (1.0 - 2.0 * u2) * std::exp(-u2);
}

BScan SyntheticGenerator::generate_bscan(const SyntheticSurveyConfig& cfg) {
    BScan bscan{};
    bscan.metadata.source_format = "SYNTHETIC";
    bscan.metadata.sample_interval_ns = cfg.sample_interval_ns;
    bscan.metadata.samples_per_trace = cfg.samples_per_trace;
    bscan.metadata.time_window_ns =
        cfg.sample_interval_ns * static_cast<double>(cfg.samples_per_trace);
    bscan.metadata.time_zero_ns = cfg.time_zero_ns;
    bscan.metadata.trace_spacing_m = cfg.trace_spacing_m;
    bscan.metadata.scans_per_second = cfg.scans_per_second;
    bscan.metadata.antenna_freq_mhz = cfg.antenna_freq_mhz;
    bscan.metadata.relative_permittivity = cfg.relative_permittivity;
    bscan.metadata.description = "Synthetic Georeferenced GPR Profile";

    const double eps_r = std::max(1.0, cfg.relative_permittivity);
    const double v_m_ns = kSpeedOfLightMPerNs / std::sqrt(eps_r);
    const double dt = cfg.sample_interval_ns;
    const std::size_t ns = cfg.samples_per_trace;

    std::mt19937 rng(cfg.random_seed);
    std::normal_distribution<double> noise_dist(0.0, cfg.noise_std);

    EnuCoord current_enu{0.0, 0.0, 0.0};
    bscan.traces.reserve(cfg.num_traces);

    for (std::size_t i = 0; i < cfg.num_traces; ++i) {
        const double x_m = static_cast<double>(i) * cfg.trace_spacing_m;
        const double heading_deg = cfg.azimuth_deg + cfg.curvature_deg_per_m * x_m;
        const double heading_rad = heading_deg * (std::numbers::pi / 180.0);

        if (i > 0) {
            current_enu.east_m += cfg.trace_spacing_m * std::sin(heading_rad);
            current_enu.north_m += cfg.trace_spacing_m * std::cos(heading_rad);
            // Gentle realistic street elevation undulation
            current_enu.up_m = 0.12 * std::sin(0.35 * x_m);
        }

        AScan tr{};
        tr.trace_id = i;
        tr.distance_along_track_m = x_m;
        tr.timestamp_sec = static_cast<double>(i) / std::max(1.0, cfg.scans_per_second);
        tr.heading_deg = std::fmod(heading_deg + 360.0, 360.0);
        tr.position_wgs84 = geo::enu_to_wgs84(current_enu, cfg.start_wgs84);
        tr.samples.assign(ns, 0.0);

        for (std::size_t j = 0; j < ns; ++j) {
            const double t_ns = static_cast<double>(j) * dt;
            const double t_sub = t_ns - cfg.time_zero_ns;
            double val = 0.0;

            // 1. Direct air/ground coupling wave arriving at time_zero_ns
            if (cfg.include_direct_wave) {
                val += 1.60 * ricker_wavelet(t_sub, cfg.antenna_freq_mhz);
            }

            // 2. Low-frequency inductive wow drift + DC offset
            if (cfg.include_wow_drift) {
                val += 0.25 + 0.35 * std::exp(-t_ns / 14.0) * std::cos(0.18 * t_ns);
            }

            // 3. Stratigraphic dipping horizons
            for (const auto& hz : cfg.horizons) {
                const double z_hz = hz.base_depth_m + hz.slope_m_per_m * x_m;
                if (z_hz <= 0.02) {
                    continue;
                }
                const double twt_hz = (2.0 * z_hz) / v_m_ns;
                const double atten = std::exp(-cfg.attenuation_np_per_ns * twt_hz);
                val += hz.reflectivity * atten *
                       ricker_wavelet(t_sub - twt_hz, cfg.antenna_freq_mhz);
            }

            // 4. Buried cylindrical pipes / fiber-optic conduits (diffraction hyperbolas)
            for (const auto& tgt : cfg.targets) {
                const double dx = x_m - tgt.position_along_track_m;
                const double z_center = tgt.depth_m + tgt.radius_m;
                const double R_center = std::hypot(dx, z_center);
                const double R_surface = std::max(0.02, R_center - tgt.radius_m);
                const double twt_tgt = (2.0 * R_surface) / v_m_ns;
                const double spreading = 1.0 / std::sqrt(std::max(0.15, R_surface));
                const double obliquity = z_center / R_center;
                const double atten = std::exp(-cfg.attenuation_np_per_ns * twt_tgt);

                val += tgt.reflectivity * spreading * obliquity * atten *
                       ricker_wavelet(t_sub - twt_tgt, cfg.antenna_freq_mhz);
            }

            // 5. Thermal receiver noise
            if (cfg.noise_std > 0.0) {
                val += noise_dist(rng);
            }

            tr.samples[j] = val * 10000.0;  // Realistic 16-bit ADC counts scale
        }

        bscan.traces.push_back(std::move(tr));
    }

    return bscan;
}

std::vector<BScan> SyntheticGenerator::generate_3d_grid(
    std::size_t num_parallel_lines,
    double line_spacing_m,
    const SyntheticSurveyConfig& base_cfg) {
    std::vector<BScan> grid;
    grid.reserve(num_parallel_lines);

    const double perp_az_rad = (base_cfg.azimuth_deg + 90.0) * (std::numbers::pi / 180.0);

    for (std::size_t k = 0; k < num_parallel_lines; ++k) {
        SyntheticSurveyConfig line_cfg = base_cfg;
        line_cfg.random_seed = base_cfg.random_seed + static_cast<std::uint32_t>(k * 101);

        const double cross_offset_m = static_cast<double>(k) * line_spacing_m;
        const EnuCoord start_offset{
            .east_m = cross_offset_m * std::sin(perp_az_rad),
            .north_m = cross_offset_m * std::cos(perp_az_rad),
            .up_m = 0.0
        };
        line_cfg.start_wgs84 = geo::enu_to_wgs84(start_offset, base_cfg.start_wgs84);

        // Shift oblique conduits realistically across parallel lines
        for (std::size_t t = 0; t < line_cfg.targets.size(); ++t) {
            const double skew = (t % 2 == 0) ? 0.25 : -0.15;
            line_cfg.targets[t].position_along_track_m += skew * cross_offset_m;
        }

        grid.push_back(generate_bscan(line_cfg));
    }

    return grid;
}

}  // namespace gpr3d::sim

