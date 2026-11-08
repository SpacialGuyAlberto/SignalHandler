#include "gpr3d/geo/trajectory_sync.hpp"
#include "gpr3d/geo/crs_transform.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace gpr3d::geo {

namespace {

struct KeyframeEnu {
    double param{};  // Either trace index, timestamp (s), or normalized [0, 1]
    EnuCoord enu{};
};

EnuCoord interpolate_enu(const std::vector<KeyframeEnu>& keys, double q) noexcept {
    if (keys.empty()) {
        return {};
    }
    if (keys.size() == 1 || q <= keys.front().param) {
        return keys.front().enu;
    }
    if (q >= keys.back().param) {
        return keys.back().enu;
    }

    auto it = std::upper_bound(
        keys.begin(), keys.end(), q,
        [](double val, const KeyframeEnu& kf) { return val < kf.param; });
    const std::size_t idx1 = static_cast<std::size_t>(std::distance(keys.begin(), it));
    const std::size_t idx0 = idx1 - 1;

    const double p0 = keys[idx0].param;
    const double p1 = keys[idx1].param;
    const double alpha = (p1 > p0) ? (q - p0) / (p1 - p0) : 0.0;

    return EnuCoord{
        .east_m = keys[idx0].enu.east_m + alpha * (keys[idx1].enu.east_m - keys[idx0].enu.east_m),
        .north_m = keys[idx0].enu.north_m + alpha * (keys[idx1].enu.north_m - keys[idx0].enu.north_m),
        .up_m = keys[idx0].enu.up_m + alpha * (keys[idx1].enu.up_m - keys[idx0].enu.up_m)
    };
}

}  // namespace

void update_headings_and_distances(BScan& bscan, const LeverArmOffset& lever_arm) {
    const std::size_t n = bscan.traces.size();
    if (n == 0) {
        return;
    }

    // Compute headings from neighbor differences along the trajectory
    double last_valid_heading = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!bscan.traces[i].position_wgs84.has_value()) {
            continue;
        }
        std::size_t i0 = (i > 0) ? (i - 1) : i;
        std::size_t i1 = (i + 1 < n) ? (i + 1) : i;
        while (i1 < n && bscan.traces[i0].position_wgs84.has_value() &&
               bscan.traces[i1].position_wgs84.has_value() &&
               horizontal_distance_m(*bscan.traces[i0].position_wgs84,
                                     *bscan.traces[i1].position_wgs84) < 1e-4 &&
               (i0 > 0 || i1 + 1 < n)) {
            if (i0 > 0) --i0;
            if (i1 + 1 < n) ++i1;
        }
        if (i0 != i1 && bscan.traces[i0].position_wgs84.has_value() &&
            bscan.traces[i1].position_wgs84.has_value()) {
            const double dist = horizontal_distance_m(
                *bscan.traces[i0].position_wgs84, *bscan.traces[i1].position_wgs84);
            if (dist >= 1e-5) {
                last_valid_heading = forward_azimuth_deg(
                    *bscan.traces[i0].position_wgs84, *bscan.traces[i1].position_wgs84);
            }
        }
        bscan.traces[i].heading_deg = last_valid_heading;
    }

    // Apply lever-arm offset if non-zero
    if (lever_arm.forward_m != 0.0 || lever_arm.right_m != 0.0 || lever_arm.up_m != 0.0) {
        for (auto& trace : bscan.traces) {
            if (trace.position_wgs84.has_value()) {
                trace.position_wgs84 = apply_lever_arm(
                    *trace.position_wgs84,
                    trace.heading_deg,
                    lever_arm.forward_m,
                    lever_arm.right_m,
                    lever_arm.up_m);
            }
        }
    }

    // Recompute cumulative horizontal distance along track
    double cum_dist = 0.0;
    bscan.traces[0].distance_along_track_m = 0.0;
    for (std::size_t i = 1; i < n; ++i) {
        if (bscan.traces[i - 1].position_wgs84.has_value() &&
            bscan.traces[i].position_wgs84.has_value()) {
            cum_dist += horizontal_distance_m(
                *bscan.traces[i - 1].position_wgs84, *bscan.traces[i].position_wgs84);
        } else {
            cum_dist += bscan.metadata.trace_spacing_m;
        }
        bscan.traces[i].distance_along_track_m = cum_dist;
    }

    if (n > 1 && cum_dist > 0.0) {
        bscan.metadata.trace_spacing_m = cum_dist / static_cast<double>(n - 1);
    }
}

void synchronize_trajectory(
    BScan& bscan,
    std::span<const GpsFix> gps_fixes,
    const LeverArmOffset& lever_arm) {
    if (bscan.empty()) {
        return;
    }
    if (gps_fixes.empty()) {
        throw std::invalid_argument("synchronize_trajectory: gps_fixes cannot be empty");
    }

    const Wgs84Coord origin = gps_fixes.front().wgs84;
    const std::size_t n_traces = bscan.num_traces();

    const bool all_have_trace_idx = std::all_of(
        gps_fixes.begin(), gps_fixes.end(),
        [](const GpsFix& f) { return f.trace_index.has_value(); });

    const bool fixes_have_time = std::all_of(
        gps_fixes.begin(), gps_fixes.end(),
        [](const GpsFix& f) { return f.timestamp_sec.has_value(); });

    const bool traces_have_time = std::all_of(
        bscan.traces.begin(), bscan.traces.end(),
        [](const AScan& t) { return t.timestamp_sec.has_value(); });

    std::vector<KeyframeEnu> keys;
    keys.reserve(gps_fixes.size());

    if (all_have_trace_idx) {
        for (const auto& fix : gps_fixes) {
            keys.push_back(KeyframeEnu{
                .param = static_cast<double>(*fix.trace_index),
                .enu = wgs84_to_enu(fix.wgs84, origin)
            });
        }
        std::sort(keys.begin(), keys.end(),
                  [](const KeyframeEnu& a, const KeyframeEnu& b) { return a.param < b.param; });

        for (std::size_t i = 0; i < n_traces; ++i) {
            const EnuCoord enu = interpolate_enu(keys, static_cast<double>(i));
            bscan.traces[i].position_wgs84 = enu_to_wgs84(enu, origin);
        }
    } else if (fixes_have_time && (traces_have_time || bscan.metadata.scans_per_second > 0.0)) {
        for (const auto& fix : gps_fixes) {
            keys.push_back(KeyframeEnu{
                .param = *fix.timestamp_sec,
                .enu = wgs84_to_enu(fix.wgs84, origin)
            });
        }
        std::sort(keys.begin(), keys.end(),
                  [](const KeyframeEnu& a, const KeyframeEnu& b) { return a.param < b.param; });

        const double t0 = keys.front().param;
        for (std::size_t i = 0; i < n_traces; ++i) {
            const double t_query = traces_have_time
                                       ? *bscan.traces[i].timestamp_sec
                                       : (t0 + static_cast<double>(i) / bscan.metadata.scans_per_second);
            bscan.traces[i].timestamp_sec = t_query;
            const EnuCoord enu = interpolate_enu(keys, t_query);
            bscan.traces[i].position_wgs84 = enu_to_wgs84(enu, origin);
        }
    } else {
        // Proportional mapping along fix sequence [0, 1]
        const std::size_t m = gps_fixes.size();
        for (std::size_t k = 0; k < m; ++k) {
            const double u = (m > 1) ? static_cast<double>(k) / static_cast<double>(m - 1) : 0.0;
            keys.push_back(KeyframeEnu{
                .param = u,
                .enu = wgs84_to_enu(gps_fixes[k].wgs84, origin)
            });
        }
        for (std::size_t i = 0; i < n_traces; ++i) {
            const double u = (n_traces > 1)
                                 ? static_cast<double>(i) / static_cast<double>(n_traces - 1)
                                 : 0.0;
            const EnuCoord enu = interpolate_enu(keys, u);
            bscan.traces[i].position_wgs84 = enu_to_wgs84(enu, origin);
        }
    }

    update_headings_and_distances(bscan, lever_arm);
}

void assign_linear_trajectory(
    BScan& bscan,
    const Wgs84Coord& origin,
    double azimuth_deg,
    const LeverArmOffset& lever_arm) {
    if (bscan.empty()) {
        return;
    }
    const double az_rad = azimuth_deg * (std::numbers::pi / 180.0);
    const double sin_az = std::sin(az_rad);
    const double cos_az = std::cos(az_rad);
    const double step = (bscan.metadata.trace_spacing_m > 0.0) ? bscan.metadata.trace_spacing_m : 0.02;

    for (std::size_t i = 0; i < bscan.traces.size(); ++i) {
        const double dist = static_cast<double>(i) * step;
        const EnuCoord enu{
            .east_m = dist * sin_az,
            .north_m = dist * cos_az,
            .up_m = 0.0
        };
        bscan.traces[i].position_wgs84 = enu_to_wgs84(enu, origin);
        bscan.traces[i].heading_deg = azimuth_deg;
    }

    update_headings_and_distances(bscan, lever_arm);
}

}  // namespace gpr3d::geo

