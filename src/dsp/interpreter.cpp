#include "gpr3d/dsp/interpreter.hpp"
#include "gpr3d/dsp/fft.hpp"
#include "gpr3d/geo/crs_transform.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace gpr3d::dsp {

double RadarInterpreter::twt_to_depth_m(
    double twt_ns,
    double default_epsr,
    double antenna_separation_m,
    const std::vector<DielectricLayer>& layers) noexcept {
    if (twt_ns <= 0.0) {
        return 0.0;
    }

    if (layers.empty()) {
        const double eps_r = std::max(1.0, default_epsr);
        const double v = kSpeedOfLightMPerNs / std::sqrt(eps_r);
        const double half_path = 0.5 * v * twt_ns;
        if (antenna_separation_m > 0.0) {
            const double half_sep = 0.5 * antenna_separation_m;
            return (half_path > half_sep)
                       ? std::sqrt(half_path * half_path - half_sep * half_sep)
                       : 0.0;
        }
        return half_path;
    }

    double rem_twt = twt_ns;
    double depth = 0.0;
    for (std::size_t k = 0; k < layers.size(); ++k) {
        const double v_k = kSpeedOfLightMPerNs / std::sqrt(std::max(1.0, layers[k].relative_permittivity));
        if (k + 1 == layers.size()) {
            depth += 0.5 * v_k * rem_twt;
            break;
        }
        const double dz = std::max(0.0, layers[k].thickness_m);
        const double layer_twt = (2.0 * dz) / v_k;
        if (rem_twt <= layer_twt) {
            depth += 0.5 * v_k * rem_twt;
            break;
        }
        depth += dz;
        rem_twt -= layer_twt;
    }
    return depth;
}

void RadarInterpreter::append_bscan_to_3d(
    PointCloud3D& cloud,
    const BScan& bscan,
    const InterpretationConfig& cfg) {
    if (bscan.empty()) {
        return;
    }

    const std::size_t n_traces = bscan.num_traces();
    const std::size_t ns = bscan.num_samples();
    if (ns == 0) {
        return;
    }

    const double dt = bscan.metadata.sample_interval_ns;
    const double t0 = bscan.metadata.time_zero_ns;
    const double eps_r = cfg.override_epsr.value_or(bscan.metadata.relative_permittivity);

    // Prepare surface WGS84 coordinates for each trace (synthesizing fallback line if absent)
    std::vector<Wgs84Coord> surface_coords(n_traces);
    const double step_m = (bscan.metadata.trace_spacing_m > 0.0) ? bscan.metadata.trace_spacing_m : 0.02;
    const double az_rad = cfg.fallback_azimuth_deg * (std::numbers::pi / 180.0);

    for (std::size_t i = 0; i < n_traces; ++i) {
        if (bscan.traces[i].position_wgs84.has_value()) {
            surface_coords[i] = *bscan.traces[i].position_wgs84;
        } else {
            const double d = (bscan.traces[i].distance_along_track_m > 0.0 || i == 0)
                                 ? bscan.traces[i].distance_along_track_m
                                 : (static_cast<double>(i) * step_m);
            const EnuCoord enu{
                .east_m = d * std::sin(az_rad),
                .north_m = d * std::cos(az_rad),
                .up_m = 0.0
            };
            surface_coords[i] = geo::enu_to_wgs84(enu, cfg.fallback_origin_wgs84);
        }
    }

    // Initialize cloud reference origin & UTM zone on first B-scan
    if (!cloud.has_valid_crs) {
        cloud.origin_wgs84 = surface_coords.front();
        cloud.origin_utm = geo::wgs84_to_utm(cloud.origin_wgs84, cfg.forced_utm_zone);
        cloud.has_valid_crs = true;
        cloud.source_description = bscan.metadata.description;
    }
    const int fixed_zone = cfg.forced_utm_zone.value_or(cloud.origin_utm.zone);

    // Precompute vertical depth table and ensure envelopes are available
    std::vector<double> depth_table(ns, 0.0);
    std::vector<double> twt_table(ns, 0.0);
    for (std::size_t j = 0; j < ns; ++j) {
        const double twt = static_cast<double>(j) * dt - t0;
        twt_table[j] = std::max(0.0, twt);
        depth_table[j] = twt_to_depth_m(
            twt, eps_r, bscan.metadata.antenna_separation_m, cfg.layers);
    }

    std::vector<std::vector<double>> envelopes(n_traces);
    double max_env = 0.0;
    for (std::size_t i = 0; i < n_traces; ++i) {
        if (bscan.traces[i].envelope.size() == bscan.traces[i].samples.size() &&
            !bscan.traces[i].envelope.empty()) {
            envelopes[i] = bscan.traces[i].envelope;
        } else {
            envelopes[i] = hilbert_envelope(bscan.traces[i].samples);
        }

        const std::size_t m = std::min(ns, envelopes[i].size());
        for (std::size_t j = 0; j < m; ++j) {
            if (depth_table[j] >= cfg.min_depth_m && depth_table[j] <= cfg.max_depth_m) {
                max_env = std::max(max_env, envelopes[i][j]);
            }
        }
    }
    if (max_env <= 0.0) {
        max_env = 1.0;
    }

    const std::size_t t_stride = std::max<std::size_t>(1, cfg.trace_stride);
    const std::size_t v_stride = std::max<std::size_t>(1, cfg.vertical_stride);
    const double cutoff = (cfg.mode == ExtractionMode::FullCurtain)
                              ? 0.0
                              : std::clamp(cfg.min_intensity_threshold, 0.0, 1.0);

    for (std::size_t i = 0; i < n_traces; i += t_stride) {
        const auto& tr = bscan.traces[i];
        const auto& env = envelopes[i];
        const std::size_t m = std::min({ns, tr.samples.size(), env.size()});

        const Wgs84Coord& surf_wgs = surface_coords[i];
        const UtmCoord surf_utm = geo::wgs84_to_utm(surf_wgs, fixed_zone);
        const EnuCoord surf_enu = geo::wgs84_to_enu(surf_wgs, cloud.origin_wgs84);

        for (std::size_t j = 0; j < m; j += v_stride) {
            const double depth = depth_table[j];
            if (depth < cfg.min_depth_m || depth > cfg.max_depth_m) {
                continue;
            }

            const double norm_i = std::clamp(env[j] / max_env, 0.0, 1.0);
            const bool is_local_peak =
                (j > 0 && j + 1 < m) && (env[j] > env[j - 1]) && (env[j] >= env[j + 1]);

            if (cfg.mode == ExtractionMode::ReflectorPeaksOnly && !is_local_peak) {
                continue;
            }
            if (norm_i < cutoff) {
                continue;
            }

            Point3D pt{};
            pt.wgs84 = Wgs84Coord{
                .latitude_deg = surf_wgs.latitude_deg,
                .longitude_deg = surf_wgs.longitude_deg,
                .altitude_m = surf_wgs.altitude_m - depth
            };
            pt.utm = UtmCoord{
                .easting_m = surf_utm.easting_m,
                .northing_m = surf_utm.northing_m,
                .elevation_m = surf_utm.elevation_m - depth,
                .zone = surf_utm.zone,
                .is_northern = surf_utm.is_northern
            };
            pt.enu = EnuCoord{
                .east_m = surf_enu.east_m,
                .north_m = surf_enu.north_m,
                .up_m = surf_enu.up_m - depth
            };

            pt.depth_m = depth;
            pt.twt_ns = twt_table[j];
            pt.amplitude = tr.samples[j];
            pt.envelope = env[j];
            pt.intensity = static_cast<float>(norm_i);
            pt.trace_id = tr.trace_id;
            pt.sample_idx = j;
            pt.is_peak = is_local_peak;

            cloud.points.push_back(pt);
        }
    }
}

PointCloud3D RadarInterpreter::transform_to_3d(
    const BScan& bscan,
    const InterpretationConfig& cfg) {
    PointCloud3D cloud{};
    append_bscan_to_3d(cloud, bscan, cfg);
    return cloud;
}

}  // namespace gpr3d::dsp

