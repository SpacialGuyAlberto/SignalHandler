#include "gpr3d/dsp/object_transformer.hpp"
#include "gpr3d/dsp/fft.hpp"
#include "gpr3d/geo/crs_transform.hpp"
#include "gpr3d/geo/trajectory_sync.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <numbers>
#include <sstream>
#include <vector>

namespace gpr3d::dsp {

namespace {

using scene::ColorRgba;
using scene::Mesh3D;
using scene::PolarityKind;
using scene::SemanticKind;
using scene::Vec3;

Vec3 to_vec3(const EnuCoord& e) noexcept {
    return {e.east_m, e.north_m, e.up_m};
}

void recompute_smooth_normals(Mesh3D& mesh) {
    mesh.normals.assign(mesh.vertices.size(), Vec3{0.0, 0.0, 0.0});
    for (const auto& tri : mesh.triangles) {
        const Vec3& v0 = mesh.vertices[tri[0]];
        const Vec3& v1 = mesh.vertices[tri[1]];
        const Vec3& v2 = mesh.vertices[tri[2]];
        const Vec3 fn = Vec3::cross(v1 - v0, v2 - v0);
        mesh.normals[tri[0]] = mesh.normals[tri[0]] + fn;
        mesh.normals[tri[1]] = mesh.normals[tri[1]] + fn;
        mesh.normals[tri[2]] = mesh.normals[tri[2]] + fn;
    }
    for (auto& n : mesh.normals) {
        n = n.normalized();
    }
}

Mesh3D build_surface_from_profiles(
    const std::string& name,
    SemanticKind kind,
    const std::vector<std::vector<EnuCoord>>& line_profiles,
    double single_line_half_width_m,
    double fallback_heading_deg,
    const ColorRgba& color) {
    Mesh3D mesh{};
    mesh.name = name;
    mesh.kind = kind;
    mesh.color = color;
    mesh.metallic = 0.05f;
    mesh.roughness = 0.85f;

    if (line_profiles.empty() || line_profiles.front().size() < 2) {
        return mesh;
    }

    std::vector<std::vector<Vec3>> rows;
    if (line_profiles.size() == 1) {
        // Extrude single profile into left and right edges perpendicular to local tangent
        const auto& prof = line_profiles.front();
        const std::size_t m = prof.size();
        std::vector<Vec3> left_row(m);
        std::vector<Vec3> right_row(m);

        const double fb_rad = (fallback_heading_deg + 90.0) * (std::numbers::pi / 180.0);
        Vec3 last_perp{std::sin(fb_rad), std::cos(fb_rad), 0.0};

        for (std::size_t i = 0; i < m; ++i) {
            const Vec3 p = to_vec3(prof[i]);
            const std::size_t i0 = (i > 0) ? (i - 1) : i;
            const std::size_t i1 = (i + 1 < m) ? (i + 1) : i;
            Vec3 tang = to_vec3(prof[i1]) - to_vec3(prof[i0]);
            tang.z = 0.0;
            if (tang.norm() > 1e-6) {
                tang = tang.normalized();
                // Right-hand perpendicular in East-North plane: (t_y, -t_x, 0)
                last_perp = Vec3{tang.y, -tang.x, 0.0};
            }
            left_row[i] = p - last_perp * single_line_half_width_m;
            right_row[i] = p + last_perp * single_line_half_width_m;
        }
        rows.push_back(std::move(left_row));
        rows.push_back(std::move(right_row));
    } else {
        // Resample/truncate all parallel line profiles to common station count M
        std::size_t min_m = line_profiles.front().size();
        for (const auto& p : line_profiles) {
            min_m = std::min(min_m, p.size());
        }
        if (min_m < 2) {
            return mesh;
        }
        rows.resize(line_profiles.size(), std::vector<Vec3>(min_m));
        for (std::size_t r = 0; r < line_profiles.size(); ++r) {
            for (std::size_t c = 0; c < min_m; ++c) {
                rows[r][c] = to_vec3(line_profiles[r][c]);
            }
        }
    }

    const std::size_t R = rows.size();
    const std::size_t C = rows.front().size();
    mesh.vertices.reserve(R * C);
    for (std::size_t r = 0; r < R; ++r) {
        for (std::size_t c = 0; c < C; ++c) {
            mesh.vertices.push_back(rows[r][c]);
        }
    }

    mesh.triangles.reserve((R - 1) * (C - 1) * 2);
    for (std::size_t r = 0; r + 1 < R; ++r) {
        for (std::size_t c = 0; c + 1 < C; ++c) {
            const auto i00 = static_cast<std::uint32_t>(r * C + c);
            const auto i01 = static_cast<std::uint32_t>(r * C + c + 1);
            const auto i10 = static_cast<std::uint32_t>((r + 1) * C + c);
            const auto i11 = static_cast<std::uint32_t>((r + 1) * C + c + 1);
            mesh.triangles.push_back({i00, i10, i11});
            mesh.triangles.push_back({i00, i11, i01});
        }
    }

    recompute_smooth_normals(mesh);
    return mesh;
}

struct TrackedHorizon1D {
    std::vector<std::size_t> sample_indices;
    std::vector<double> depths_m;
    double mean_depth_m{0.0};
    double slope_m_per_m{0.0};
    double mean_norm_intensity{0.0};
};

std::vector<TrackedHorizon1D> detect_horizons_in_bscan(
    const BScan& strat_bscan,
    double min_depth_m,
    double max_depth_m,
    double min_coverage,
    double rel_threshold) {
    const std::size_t n_traces = strat_bscan.num_traces();
    const std::size_t ns = strat_bscan.num_samples();
    if (n_traces < 10 || ns < 16) {
        return {};
    }

    const double dt = strat_bscan.metadata.sample_interval_ns;
    const double eps_r = std::max(1.0, strat_bscan.metadata.relative_permittivity);
    const double v_m_ns = kSpeedOfLightMPerNs / std::sqrt(eps_r);

    std::size_t j_min = 1;
    std::size_t j_max = ns - 2;
    for (std::size_t j = 0; j < ns; ++j) {
        const double d = 0.5 * v_m_ns * static_cast<double>(j) * dt;
        if (d < min_depth_m) {
            j_min = j + 1;
        }
        if (d <= max_depth_m) {
            j_max = j;
        }
    }
    if (j_min + 4 >= j_max) {
        return {};
    }

    // Laterally filter envelopes with a running median (erases localized pipe hyperbolas
    // while preserving flat and gently dipping stratigraphic layers!)
    const std::size_t half_med_w = std::clamp<std::size_t>(n_traces / 10, 3, 16);
    std::vector<std::vector<double>> sm_env(n_traces, std::vector<double>(ns, 0.0));
    std::vector<double> win_buf;
    win_buf.reserve(2 * half_med_w + 1);
    double max_env = 0.0;
    for (std::size_t j = j_min; j <= j_max; ++j) {
        for (std::size_t i = 0; i < n_traces; ++i) {
            const std::size_t i0 = (i > half_med_w) ? (i - half_med_w) : 0;
            const std::size_t i1 = std::min(n_traces - 1, i + half_med_w);
            win_buf.clear();
            for (std::size_t k = i0; k <= i1; ++k) {
                win_buf.push_back(strat_bscan.traces[k].envelope[j]);
            }
            const auto mid = win_buf.begin() + static_cast<std::ptrdiff_t>(win_buf.size() / 2);
            std::nth_element(win_buf.begin(), mid, win_buf.end());
            sm_env[i][j] = *mid;
            max_env = std::max(max_env, sm_env[i][j]);
        }
    }
    if (max_env <= 0.0) {
        return {};
    }

    // Median envelope profile across traces to find stable horizon seed depths
    std::vector<double> med_profile(ns, 0.0);
    std::vector<double> col_buf(n_traces);
    for (std::size_t j = j_min; j <= j_max; ++j) {
        for (std::size_t i = 0; i < n_traces; ++i) {
            col_buf[i] = sm_env[i][j];
        }
        std::nth_element(col_buf.begin(), col_buf.begin() + static_cast<std::ptrdiff_t>(n_traces / 2), col_buf.end());
        med_profile[j] = col_buf[n_traces / 2];
    }

    // Estimate background median floor and highest interior local peak (ignores monotonic direct-wave tail at j_min)
    std::vector<double> valid_meds(med_profile.begin() + static_cast<std::ptrdiff_t>(j_min),
                                   med_profile.begin() + static_cast<std::ptrdiff_t>(j_max + 1));
    std::nth_element(valid_meds.begin(), valid_meds.begin() + static_cast<std::ptrdiff_t>(valid_meds.size() / 2), valid_meds.end());
    const double med_noise_floor = valid_meds[valid_meds.size() / 2];

    double max_peak_med = 0.0;
    for (std::size_t j = j_min + 2; j + 2 <= j_max; ++j) {
        if (med_profile[j] > med_profile[j - 1] && med_profile[j] >= med_profile[j + 1]) {
            max_peak_med = std::max(max_peak_med, med_profile[j]);
        }
    }
    if (max_peak_med <= 0.0) {
        return {};
    }
    const double seed_cutoff = std::max(rel_threshold * max_peak_med, 3.5 * med_noise_floor);

    const auto min_sep_samples = static_cast<std::size_t>(
        std::max(6L, std::lround(0.18 / (0.5 * v_m_ns * dt))));

    std::vector<std::size_t> candidate_seeds;
    for (std::size_t j = j_min + 2; j + 2 <= j_max; ++j) {
        if (med_profile[j] >= seed_cutoff &&
            med_profile[j] > med_profile[j - 1] &&
            med_profile[j] >= med_profile[j + 1]) {
            candidate_seeds.push_back(j);
        }
    }
    std::sort(candidate_seeds.begin(), candidate_seeds.end(),
              [&](std::size_t a, std::size_t b) { return med_profile[a] > med_profile[b]; });

    std::vector<TrackedHorizon1D> horizons;
    std::vector<bool> occupied_depth(ns, false);
    const std::size_t mid_i = n_traces / 2;

    for (std::size_t seed_j : candidate_seeds) {
        if (occupied_depth[seed_j] || horizons.size() >= 4) {
            continue;
        }

        // Refine seed at middle trace within +-min_sep_samples/2
        std::size_t best_mid_j = seed_j;
        const std::size_t s_lo = (seed_j > min_sep_samples / 2) ? (seed_j - min_sep_samples / 2) : j_min;
        const std::size_t s_hi = std::min(j_max, seed_j + min_sep_samples / 2);
        for (std::size_t j = s_lo; j <= s_hi; ++j) {
            if (sm_env[mid_i][j] > sm_env[mid_i][best_mid_j]) {
                best_mid_j = j;
            }
        }

        std::vector<std::size_t> track(n_traces, best_mid_j);
        // Track forward
        for (std::size_t i = mid_i + 1; i < n_traces; ++i) {
            const std::size_t prev = track[i - 1];
            const std::size_t a = std::max(j_min, (prev > 2) ? (prev - 2) : j_min);
            const std::size_t b = std::min(j_max, prev + 2);
            std::size_t bj = prev;
            for (std::size_t j = a; j <= b; ++j) {
                if (sm_env[i][j] > sm_env[i][bj]) bj = j;
            }
            track[i] = bj;
        }
        // Track backward
        for (std::size_t i = mid_i; i > 0; --i) {
            const std::size_t prev = track[i];
            const std::size_t a = std::max(j_min, (prev > 2) ? (prev - 2) : j_min);
            const std::size_t b = std::min(j_max, prev + 2);
            std::size_t bj = prev;
            for (std::size_t j = a; j <= b; ++j) {
                if (sm_env[i - 1][j] > sm_env[i - 1][bj]) bj = j;
            }
            track[i - 1] = bj;
        }

        // Evaluate lateral continuity & linear regression residual to reject pipe hyperbolas
        std::size_t active_count = 0;
        double sum_x = 0.0, sum_z = 0.0, sum_xx = 0.0, sum_xz = 0.0, sum_int = 0.0;
        std::vector<double> depths(n_traces, 0.0);

        for (std::size_t i = 0; i < n_traces; ++i) {
            const double x = strat_bscan.traces[i].distance_along_track_m;
            const double z = 0.5 * v_m_ns * static_cast<double>(track[i]) * dt;
            depths[i] = z;
            const double norm_e = sm_env[i][track[i]] / max_env;
            sum_int += norm_e;
            if (sm_env[i][track[i]] >= 0.50 * med_profile[seed_j]) {
                ++active_count;
            }
            sum_x += x;
            sum_z += z;
            sum_xx += x * x;
            sum_xz += x * z;
        }

        const double coverage = static_cast<double>(active_count) / static_cast<double>(n_traces);
        if (coverage < min_coverage) {
            continue;
        }

        const double n_d = static_cast<double>(n_traces);
        const double denom = n_d * sum_xx - sum_x * sum_x;
        const double slope = (std::abs(denom) > 1e-9) ? ((n_d * sum_xz - sum_x * sum_z) / denom) : 0.0;
        const double intercept = (sum_z - slope * sum_x) / n_d;

        double sq_err = 0.0;
        for (std::size_t i = 0; i < n_traces; ++i) {
            const double x = strat_bscan.traces[i].distance_along_track_m;
            const double diff = depths[i] - (intercept + slope * x);
            sq_err += diff * diff;
        }
        const double rms_res = std::sqrt(sq_err / n_d);
        if (rms_res > 0.08) {
            continue;  // Rejects high-curvature diffraction hyperbolas
        }

        // Mark depth exclusion zone so duplicate lobes of same wavelet are not re-picked
        for (std::size_t j = (best_mid_j > min_sep_samples ? best_mid_j - min_sep_samples : 0);
             j <= std::min(ns - 1, best_mid_j + min_sep_samples); ++j) {
            occupied_depth[j] = true;
        }

        horizons.push_back(TrackedHorizon1D{
            .sample_indices = std::move(track),
            .depths_m = std::move(depths),
            .mean_depth_m = sum_z / n_d,
            .slope_m_per_m = slope,
            .mean_norm_intensity = sum_int / n_d
        });
    }

    std::sort(horizons.begin(), horizons.end(),
              [](const TrackedHorizon1D& a, const TrackedHorizon1D& b) {
                  return a.mean_depth_m < b.mean_depth_m;
              });
    return horizons;
}

struct TargetApex2D {
    std::size_t line_idx{0};
    std::size_t trace_idx{0};
    std::size_t sample_idx{0};
    double track_x_m{0.0};
    double depth_m{0.0};
    double norm_intensity{0.0};
    double est_radius_m{0.05};
    PolarityKind polarity{PolarityKind::Mixed};
    Wgs84Coord wgs84{};
    EnuCoord enu{};
    double heading_deg{0.0};
    bool used{false};
};

}  // namespace

scene::Mesh3D SignalObjectTransformer::build_tube_mesh(
    const std::string& name,
    std::span<const EnuCoord> centerline,
    double radius_m,
    int radial_segments,
    const scene::ColorRgba& color,
    float metallic,
    float roughness) {
    Mesh3D mesh{};
    mesh.name = name;
    mesh.kind = SemanticKind::Conduit;
    mesh.color = color;
    mesh.metallic = metallic;
    mesh.roughness = roughness;

    if (centerline.size() < 2) {
        return mesh;
    }

    const int segs = std::max(6, radial_segments);
    const double r = std::max(0.015, radius_m);
    const std::size_t n_pts = centerline.size();

    // Parallel transport frames along polyline
    Vec3 prev_t = (to_vec3(centerline[1]) - to_vec3(centerline[0])).normalized();
    Vec3 ref_up = (std::abs(prev_t.z) < 0.9) ? Vec3{0.0, 0.0, 1.0} : Vec3{1.0, 0.0, 0.0};
    Vec3 u = Vec3::cross(prev_t, ref_up).normalized();
    Vec3 v = Vec3::cross(prev_t, u).normalized();

    for (std::size_t i = 0; i < n_pts; ++i) {
        Vec3 tang = prev_t;
        if (i + 1 < n_pts && i > 0) {
            tang = (to_vec3(centerline[i + 1]) - to_vec3(centerline[i - 1])).normalized();
        } else if (i + 1 < n_pts) {
            tang = (to_vec3(centerline[i + 1]) - to_vec3(centerline[i])).normalized();
        } else {
            tang = (to_vec3(centerline[i]) - to_vec3(centerline[i - 1])).normalized();
        }

        // Project previous u perpendicular to current tangent (rotation-minimizing transport)
        Vec3 u_proj = u - tang * Vec3::dot(u, tang);
        if (u_proj.norm() > 1e-6) {
            u = u_proj.normalized();
        }
        v = Vec3::cross(tang, u).normalized();
        prev_t = tang;

        const Vec3 c = to_vec3(centerline[i]);
        for (int k = 0; k < segs; ++k) {
            const double ang = 2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(segs);
            const Vec3 radial = (u * std::cos(ang) + v * std::sin(ang)).normalized();
            mesh.vertices.push_back(c + radial * r);
            mesh.normals.push_back(radial);
        }
    }

    for (std::size_t i = 0; i + 1 < n_pts; ++i) {
        const auto base0 = static_cast<std::uint32_t>(i * static_cast<std::size_t>(segs));
        const auto base1 = static_cast<std::uint32_t>((i + 1) * static_cast<std::size_t>(segs));
        for (int k = 0; k < segs; ++k) {
            const auto k_next = static_cast<std::uint32_t>((k + 1) % segs);
            const auto i00 = base0 + static_cast<std::uint32_t>(k);
            const auto i01 = base0 + k_next;
            const auto i10 = base1 + static_cast<std::uint32_t>(k);
            const auto i11 = base1 + k_next;
            mesh.triangles.push_back({i00, i10, i11});
            mesh.triangles.push_back({i00, i11, i01});
        }
    }

    // Start cap & End cap
    const Vec3 start_t = (to_vec3(centerline[1]) - to_vec3(centerline[0])).normalized();
    const auto start_center_idx = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back(to_vec3(centerline.front()));
    mesh.normals.push_back(start_t * -1.0);
    for (int k = 0; k < segs; ++k) {
        const auto k_next = static_cast<std::uint32_t>((k + 1) % segs);
        mesh.triangles.push_back({start_center_idx, k_next, static_cast<std::uint32_t>(k)});
    }

    const Vec3 end_t = (to_vec3(centerline[n_pts - 1]) - to_vec3(centerline[n_pts - 2])).normalized();
    const auto end_center_idx = static_cast<std::uint32_t>(mesh.vertices.size());
    const auto last_ring = static_cast<std::uint32_t>((n_pts - 1) * static_cast<std::size_t>(segs));
    mesh.vertices.push_back(to_vec3(centerline.back()));
    mesh.normals.push_back(end_t);
    for (int k = 0; k < segs; ++k) {
        const auto k_next = static_cast<std::uint32_t>((k + 1) % segs);
        mesh.triangles.push_back({end_center_idx, last_ring + static_cast<std::uint32_t>(k), last_ring + k_next});
    }

    return mesh;
}

scene::Mesh3D SignalObjectTransformer::build_ellipsoid_mesh(
    const std::string& name,
    const EnuCoord& center,
    const scene::Vec3& radii_m,
    int lat_rings,
    int lon_segments,
    const scene::ColorRgba& color) {
    Mesh3D mesh{};
    mesh.name = name;
    mesh.kind = SemanticKind::SubsurfaceAnomaly;
    mesh.color = color;
    mesh.metallic = 0.15f;
    mesh.roughness = 0.55f;

    const int rings = std::max(6, lat_rings);
    const int segs = std::max(8, lon_segments);
    const double rx = std::max(0.04, radii_m.x);
    const double ry = std::max(0.04, radii_m.y);
    const double rz = std::max(0.04, radii_m.z);
    const Vec3 c = to_vec3(center);

    for (int i = 0; i <= rings; ++i) {
        const double phi = std::numbers::pi * static_cast<double>(i) / static_cast<double>(rings);
        const double sin_phi = std::sin(phi);
        const double cos_phi = std::cos(phi);

        for (int j = 0; j <= segs; ++j) {
            const double theta = 2.0 * std::numbers::pi * static_cast<double>(j) / static_cast<double>(segs);
            const double ux = sin_phi * std::cos(theta);
            const double uy = sin_phi * std::sin(theta);
            const double uz = cos_phi;

            mesh.vertices.push_back(Vec3{c.x + rx * ux, c.y + ry * uy, c.z + rz * uz});
            mesh.normals.push_back(Vec3{ux / rx, uy / ry, uz / rz}.normalized());
        }
    }

    const int stride = segs + 1;
    for (int i = 0; i < rings; ++i) {
        for (int j = 0; j < segs; ++j) {
            const auto i00 = static_cast<std::uint32_t>(i * stride + j);
            const auto i01 = static_cast<std::uint32_t>(i * stride + j + 1);
            const auto i10 = static_cast<std::uint32_t>((i + 1) * stride + j);
            const auto i11 = static_cast<std::uint32_t>((i + 1) * stride + j + 1);
            mesh.triangles.push_back({i00, i10, i11});
            mesh.triangles.push_back({i00, i11, i01});
        }
    }

    return mesh;
}

scene::SignalFluctuationSummary SignalObjectTransformer::analyze_fluctuations(
    const BScan& bscan) {
    scene::SignalFluctuationSummary sum{};
    if (bscan.empty() || bscan.num_samples() == 0) {
        return sum;
    }

    const std::size_t n_traces = bscan.num_traces();
    const std::size_t ns = bscan.num_samples();
    const double dt = bscan.metadata.sample_interval_ns;
    const double eps_r = std::max(1.0, bscan.metadata.relative_permittivity);
    const double v_m_ns = kSpeedOfLightMPerNs / std::sqrt(eps_r);
    const double fs_mhz = (dt > 0.0) ? (1000.0 / dt) : 10000.0;

    sum.time_axis_ns.resize(ns);
    sum.depth_axis_m.resize(ns);
    for (std::size_t j = 0; j < ns; ++j) {
        const double t = static_cast<double>(j) * dt;
        sum.time_axis_ns[j] = t;
        sum.depth_axis_m[j] = 0.5 * v_m_ns * t;
    }

    const std::size_t n_fft = next_power_of_two(ns);
    const std::size_t n_pos_bins = n_fft / 2 + 1;
    sum.freq_axis_mhz.resize(n_pos_bins);
    for (std::size_t k = 0; k < n_pos_bins; ++k) {
        sum.freq_axis_mhz[k] = (static_cast<double>(k) * fs_mhz) / static_cast<double>(n_fft);
    }

    std::vector<double> mean_power(n_pos_bins, 0.0);
    sum.per_trace.resize(n_traces);

    double best_subsurface_peak = -1.0;
    std::size_t best_trace_idx = n_traces / 2;

    std::vector<std::complex<double>> fft_in(n_fft);
    for (std::size_t i = 0; i < n_traces; ++i) {
        const auto& tr = bscan.traces[i];
        const auto env = (!tr.envelope.empty() && tr.envelope.size() == tr.samples.size())
                             ? tr.envelope
                             : hilbert_envelope(tr.samples);

        double sq_sum = 0.0;
        double min_v = tr.samples.front();
        double max_v = tr.samples.front();
        double max_abs = 0.0;
        double peak_env = 0.0;
        double peak_env_depth = 0.0;
        double early_energy = 1e-12;
        double late_energy = 1e-12;

        for (std::size_t j = 0; j < tr.samples.size(); ++j) {
            const double v = tr.samples[j];
            sq_sum += v * v;
            min_v = std::min(min_v, v);
            max_v = std::max(max_v, v);
            max_abs = std::max(max_abs, std::abs(v));

            if (sum.depth_axis_m[j] >= 0.12 && env[j] > peak_env) {
                peak_env = env[j];
                peak_env_depth = sum.depth_axis_m[j];
            }
            if (j < tr.samples.size() / 3) {
                early_energy += v * v;
            } else if (j >= (2 * tr.samples.size()) / 3) {
                late_energy += v * v;
            }
        }

        if (peak_env > best_subsurface_peak) {
            best_subsurface_peak = peak_env;
            best_trace_idx = i;
        }

        // Trace FFT spectrum
        std::fill(fft_in.begin(), fft_in.end(), std::complex<double>{0.0, 0.0});
        for (std::size_t j = 0; j < tr.samples.size(); ++j) {
            fft_in[j] = {tr.samples[j], 0.0};
        }
        const auto spec = fft_forward(fft_in);

        double max_bin_p = 0.0;
        std::size_t dom_k = 0;
        double weighted_f = 0.0;
        double total_p = 1e-18;

        for (std::size_t k = 1; k < n_pos_bins; ++k) {
            const double p = std::norm(spec[k]);
            mean_power[k] += p;
            weighted_f += sum.freq_axis_mhz[k] * p;
            total_p += p;
            if (p > max_bin_p) {
                max_bin_p = p;
                dom_k = k;
            }
        }

        const double rms = std::sqrt(sq_sum / static_cast<double>(tr.samples.size()));
        sum.per_trace[i] = scene::TraceFluctuationMetrics{
            .trace_id = tr.trace_id,
            .distance_m = tr.distance_along_track_m,
            .rms_amplitude = rms,
            .peak_to_peak = max_v - min_v,
            .crest_factor = (rms > 1e-12) ? (max_abs / rms) : 0.0,
            .dominant_freq_mhz = sum.freq_axis_mhz[dom_k],
            .spectral_centroid_mhz = weighted_f / total_p,
            .peak_envelope_depth_m = peak_env_depth,
            .energy_decay_db = 10.0 * std::log10(early_energy / late_energy)
        };
    }

    // Normalize ensemble PSD to dB (0 dB at spectral peak)
    const double peak_psd = *std::max_element(mean_power.begin(), mean_power.end());
    sum.mean_psd_db.resize(n_pos_bins, -80.0);
    if (peak_psd > 0.0) {
        for (std::size_t k = 0; k < n_pos_bins; ++k) {
            const double ratio = std::max(1e-8, mean_power[k] / peak_psd);
            sum.mean_psd_db[k] = 10.0 * std::log10(ratio);
        }
    }

    // Detailed representative trace waveform, envelope, instantaneous phase & frequency
    sum.representative_trace_idx = best_trace_idx;
    const auto& rep_tr = bscan.traces[best_trace_idx];
    sum.rep_waveform = rep_tr.samples;
    const auto z = analytic_signal(rep_tr.samples);
    sum.rep_envelope.resize(z.size());
    sum.rep_inst_phase_rad.resize(z.size());
    sum.rep_inst_freq_mhz.assign(z.size(), 0.0);

    std::vector<double> unwrapped(z.size(), 0.0);
    for (std::size_t j = 0; j < z.size(); ++j) {
        sum.rep_envelope[j] = std::abs(z[j]);
        sum.rep_inst_phase_rad[j] = std::arg(z[j]);
        if (j == 0) {
            unwrapped[j] = sum.rep_inst_phase_rad[j];
        } else {
            double dp = sum.rep_inst_phase_rad[j] - sum.rep_inst_phase_rad[j - 1];
            while (dp > std::numbers::pi) dp -= 2.0 * std::numbers::pi;
            while (dp < -std::numbers::pi) dp += 2.0 * std::numbers::pi;
            unwrapped[j] = unwrapped[j - 1] + dp;
        }
    }
    if (dt > 0.0 && z.size() > 2) {
        for (std::size_t j = 1; j + 1 < z.size(); ++j) {
            const double dphi_dt_ghz = (unwrapped[j + 1] - unwrapped[j - 1]) / (4.0 * std::numbers::pi * dt);
            sum.rep_inst_freq_mhz[j] = std::clamp(dphi_dt_ghz * 1000.0, 0.0, 0.5 * fs_mhz);
        }
    }

    return sum;
}

scene::Scene3D SignalObjectTransformer::transform(
    const BScan& bscan,
    const ObjectTransformConfig& cfg) {
    return transform_grid(std::span<const BScan>(&bscan, 1), cfg);
}

scene::Scene3D SignalObjectTransformer::transform_grid(
    std::span<const BScan> raw_bscans,
    const ObjectTransformConfig& cfg) {
    scene::Scene3D scene{};
    if (raw_bscans.empty() || raw_bscans.front().empty()) {
        return scene;
    }

    const std::size_t n_lines = raw_bscans.size();
    std::vector<BScan> base_lines(raw_bscans.begin(), raw_bscans.end());

    // Ensure every B-scan has geodetic WGS84 coordinates
    const double perp_rad = (cfg.fallback_azimuth_deg + 90.0) * (std::numbers::pi / 180.0);
    for (std::size_t l = 0; l < n_lines; ++l) {
        const bool has_gps = std::any_of(
            base_lines[l].traces.begin(), base_lines[l].traces.end(),
            [](const AScan& t) { return t.position_wgs84.has_value(); });
        if (!has_gps) {
            const EnuCoord line_off{
                .east_m = static_cast<double>(l) * 0.50 * std::sin(perp_rad),
                .north_m = static_cast<double>(l) * 0.50 * std::cos(perp_rad),
                .up_m = 0.0
            };
            const Wgs84Coord line_origin = geo::enu_to_wgs84(line_off, cfg.fallback_origin_wgs84);
            geo::assign_linear_trajectory(base_lines[l], line_origin, cfg.fallback_azimuth_deg);
        } else {
            geo::update_headings_and_distances(base_lines[l]);
        }
    }

    scene.origin_wgs84 = *base_lines.front().traces.front().position_wgs84;
    scene.origin_utm = geo::wgs84_to_utm(scene.origin_wgs84);
    scene.has_valid_crs = true;
    scene.description = base_lines.front().metadata.description;

    // Build Ground Surface 3D reference mesh
    {
        std::vector<std::vector<EnuCoord>> ground_profiles(n_lines);
        for (std::size_t l = 0; l < n_lines; ++l) {
            const std::size_t n_tr = base_lines[l].num_traces();
            const std::size_t step = std::max<std::size_t>(1, n_tr / 50);
            for (std::size_t i = 0; i < n_tr; i += step) {
                ground_profiles[l].push_back(
                    geo::wgs84_to_enu(*base_lines[l].traces[i].position_wgs84, scene.origin_wgs84));
            }
            if ((n_tr - 1) % step != 0) {
                ground_profiles[l].push_back(
                    geo::wgs84_to_enu(*base_lines[l].traces.back().position_wgs84, scene.origin_wgs84));
            }
        }
        scene.ground_surface = build_surface_from_profiles(
            "Ground_Surface_Terrain",
            SemanticKind::GroundSurface,
            ground_profiles,
            cfg.single_line_half_length_m,
            base_lines.front().traces.front().heading_deg,
            ColorRgba{0.35f, 0.42f, 0.46f, 0.35f});
    }

    // Dual-branch processing:
    // 1) Stratigraphic branch (preserves flat/dipping horizons)
    // 2) Target branch (background removal + Kirchhoff migration to focus conduits & anomalies)
    std::vector<BScan> strat_lines = base_lines;
    std::vector<BScan> target_lines = base_lines;
    std::vector<std::vector<TrackedHorizon1D>> per_line_horizons(n_lines);

    for (std::size_t l = 0; l < n_lines; ++l) {
        if (cfg.preprocess_signals) {
            ProcessingConfig strat_cfg{};
            strat_cfg.enable_dewow = true;
            strat_cfg.enable_time_zero = true;
            strat_cfg.enable_bandpass = true;
            strat_cfg.enable_background_removal = false;  // Keep continuous horizontal strata!
            strat_cfg.enable_sec_gain = true;
            strat_cfg.max_gain = 35.0;
            strat_cfg.enable_migration = false;
            strat_cfg.compute_envelope = true;
            SignalProcessor::process(strat_lines[l], strat_cfg);
        } else if (strat_lines[l].traces.front().envelope.empty()) {
            SignalProcessor::compute_envelopes(strat_lines[l]);
        }

        if (cfg.extract_horizons) {
            per_line_horizons[l] = detect_horizons_in_bscan(
                strat_lines[l],
                cfg.min_depth_m,
                cfg.max_depth_m,
                cfg.horizon_min_coverage,
                cfg.horizon_threshold);
        }

        if (cfg.preprocess_signals) {
            ProcessingConfig tgt_cfg{};
            tgt_cfg.enable_dewow = true;
            tgt_cfg.enable_time_zero = true;
            tgt_cfg.enable_bandpass = true;
            tgt_cfg.enable_background_removal = true;
            tgt_cfg.enable_sec_gain = true;
            tgt_cfg.enable_migration = false;  // Suppress horizon dip remnants right before migration
            tgt_cfg.compute_envelope = false;
            SignalProcessor::process(target_lines[l], tgt_cfg);

            // Attenuate narrow residual bands around tracked dipping horizons so pipes stand out cleanly
            const double dt = target_lines[l].metadata.sample_interval_ns;
            const double eps_r = std::max(1.0, target_lines[l].metadata.relative_permittivity);
            const double v_m_ns = kSpeedOfLightMPerNs / std::sqrt(eps_r);
            const auto half_band = static_cast<std::size_t>(
                std::max(3L, std::lround(0.055 / (0.5 * v_m_ns * dt))));

            for (const auto& hz : per_line_horizons[l]) {
                for (std::size_t i = 0; i < target_lines[l].num_traces(); ++i) {
                    const std::size_t jc = hz.sample_indices[i];
                    const std::size_t a = (jc > half_band) ? (jc - half_band) : 0;
                    const std::size_t b = std::min(target_lines[l].num_samples() - 1, jc + half_band);
                    for (std::size_t j = a; j <= b; ++j) {
                        const double dist_norm =
                            static_cast<double>((j > jc) ? (j - jc) : (jc - j)) /
                            static_cast<double>(half_band + 1);
                        target_lines[l].traces[i].samples[j] *= (0.15 + 0.85 * dist_norm);
                    }
                }
            }

            SignalProcessor::apply_kirchhoff_migration(target_lines[l], cfg.migration_aperture_m);
            SignalProcessor::compute_envelopes(target_lines[l]);
        } else if (target_lines[l].traces.front().envelope.empty()) {
            SignalProcessor::compute_envelopes(target_lines[l]);
        }
    }

    // Reconstruct 3D Stratigraphic Horizon Objects
    if (cfg.extract_horizons && !per_line_horizons.front().empty()) {
        for (std::size_t h_idx = 0; h_idx < per_line_horizons.front().size(); ++h_idx) {
            const auto& ref_hz = per_line_horizons.front()[h_idx];
            std::vector<std::vector<EnuCoord>> hz_profiles;
            std::vector<Wgs84Coord> ref_wgs_prof;
            std::vector<EnuCoord> ref_enu_prof;

            for (std::size_t l = 0; l < n_lines; ++l) {
                // Match horizon in line l with closest mean depth
                const TrackedHorizon1D* matched = nullptr;
                double best_dz = 0.22;
                for (const auto& cand : per_line_horizons[l]) {
                    const double dz = std::abs(cand.mean_depth_m - ref_hz.mean_depth_m);
                    if (dz < best_dz) {
                        best_dz = dz;
                        matched = &cand;
                    }
                }
                if (matched == nullptr && l > 0) {
                    break;
                }
                const TrackedHorizon1D& cur = (matched != nullptr) ? *matched : ref_hz;
                const std::size_t n_tr = strat_lines[l].num_traces();
                const std::size_t step = std::max<std::size_t>(1, n_tr / 60);

                std::vector<EnuCoord> line_enu;
                for (std::size_t i = 0; i < n_tr; i += step) {
                    Wgs84Coord w = *strat_lines[l].traces[i].position_wgs84;
                    w.altitude_m -= cur.depths_m[i];
                    const EnuCoord e = geo::wgs84_to_enu(w, scene.origin_wgs84);
                    line_enu.push_back(e);
                    if (l == 0) {
                        ref_wgs_prof.push_back(w);
                        ref_enu_prof.push_back(e);
                    }
                }
                hz_profiles.push_back(std::move(line_enu));
            }

            if (hz_profiles.empty()) {
                continue;
            }

            const std::size_t obj_id = scene.horizons.size() + 1;
            const std::string name = "Stratum_Horizon_0" + std::to_string(obj_id);
            const ColorRgba color = (obj_id % 2 == 1)
                                        ? ColorRgba{0.82f, 0.64f, 0.28f, 0.62f}
                                        : ColorRgba{0.35f, 0.68f, 0.78f, 0.62f};

            scene::LayerHorizonObject3D layer_obj{};
            layer_obj.id = obj_id;
            layer_obj.label = name;
            layer_obj.mean_depth_m = ref_hz.mean_depth_m;
            layer_obj.dip_slope_m_per_m = ref_hz.slope_m_per_m;
            layer_obj.mean_intensity = ref_hz.mean_norm_intensity;
            layer_obj.profile_wgs84 = std::move(ref_wgs_prof);
            layer_obj.profile_enu = std::move(ref_enu_prof);
            layer_obj.mesh = build_surface_from_profiles(
                name,
                SemanticKind::StratigraphicLayer,
                hz_profiles,
                cfg.single_line_half_length_m,
                strat_lines.front().traces.front().heading_deg,
                color);

            if (!layer_obj.mesh.empty()) {
                scene.horizons.push_back(std::move(layer_obj));
            }
        }
    }

    // Extract 2D NMS Target Apexes per line on focused/migrated B-scans
    std::vector<std::vector<TargetApex2D>> line_apexes(n_lines);
    for (std::size_t l = 0; l < n_lines; ++l) {
        const auto& bscan = target_lines[l];
        const std::size_t n_tr = bscan.num_traces();
        const std::size_t ns = bscan.num_samples();
        const double dt = bscan.metadata.sample_interval_ns;
        const double eps_r = std::max(1.0, bscan.metadata.relative_permittivity);
        const double v_m_ns = kSpeedOfLightMPerNs / std::sqrt(eps_r);

        std::size_t j_min = 1;
        std::size_t j_max = ns - 2;
        for (std::size_t j = 0; j < ns; ++j) {
            const double d = 0.5 * v_m_ns * static_cast<double>(j) * dt;
            if (d < cfg.min_depth_m) j_min = j + 1;
            if (d <= cfg.max_depth_m) j_max = j;
        }

        double max_env = 0.0;
        for (std::size_t i = 0; i < n_tr; ++i) {
            for (std::size_t j = j_min; j <= j_max && j < bscan.traces[i].envelope.size(); ++j) {
                max_env = std::max(max_env, bscan.traces[i].envelope[j]);
            }
        }
        if (max_env <= 0.0) {
            continue;
        }

        const auto nms_i = static_cast<std::size_t>(
            std::max(3L, std::lround(0.35 / std::max(0.005, bscan.metadata.trace_spacing_m))));
        const auto nms_j = static_cast<std::size_t>(
            std::max(4L, std::lround(0.15 / (0.5 * v_m_ns * dt))));

        const double min_cand_thresh = std::min(cfg.conduit_threshold, cfg.anomaly_threshold);

        for (std::size_t i = 1; i + 1 < n_tr; ++i) {
            for (std::size_t j = j_min; j <= j_max; ++j) {
                const double val = bscan.traces[i].envelope[j];
                const double norm_v = val / max_env;
                if (norm_v < min_cand_thresh) {
                    continue;
                }

                // 2D Non-Maximum Suppression
                bool is_max = true;
                const std::size_t ia = (i > nms_i) ? (i - nms_i) : 0;
                const std::size_t ib = std::min(n_tr - 1, i + nms_i);
                const std::size_t ja = (j > nms_j) ? (j - nms_j) : j_min;
                const std::size_t jb = std::min(j_max, j + nms_j);

                for (std::size_t ii = ia; ii <= ib && is_max; ++ii) {
                    for (std::size_t jj = ja; jj <= jb; ++jj) {
                        if (ii == i && jj == j) continue;
                        if (bscan.traces[ii].envelope[jj] > val) {
                            is_max = false;
                            break;
                        }
                    }
                }
                if (!is_max) {
                    continue;
                }

                // Determine wavelet polarity in local pulse neighborhood
                const std::size_t pw = std::max<std::size_t>(2, nms_j / 2);
                double max_pos = 0.0;
                double max_neg = 0.0;
                for (std::size_t jj = (j > pw ? j - pw : 0);
                     jj <= std::min(ns - 1, j + pw); ++jj) {
                    const double s = bscan.traces[i].samples[jj];
                    if (s > max_pos) max_pos = s;
                    if (-s > max_neg) max_neg = -s;
                }
                const PolarityKind pol = (max_neg > 1.08 * max_pos)
                                             ? PolarityKind::MetallicOrHighPermittivity
                                             : PolarityKind::AirFilledDuctOrVoid;

                // Estimate effective conduit radius from envelope half-maximum vertical width
                std::size_t j_up = j;
                while (j_up > j_min && bscan.traces[i].envelope[j_up] > 0.5 * val) --j_up;
                std::size_t j_dn = j;
                while (j_dn < j_max && bscan.traces[i].envelope[j_dn] > 0.5 * val) ++j_dn;
                const double pulse_thick_m = 0.5 * v_m_ns * static_cast<double>(j_dn - j_up) * dt;
                const double est_r = std::clamp(0.45 * pulse_thick_m, 0.035, 0.18);

                const double depth = 0.5 * v_m_ns * static_cast<double>(j) * dt;
                Wgs84Coord wgs = *bscan.traces[i].position_wgs84;
                wgs.altitude_m -= depth;
                const EnuCoord enu = geo::wgs84_to_enu(wgs, scene.origin_wgs84);

                line_apexes[l].push_back(TargetApex2D{
                    .line_idx = l,
                    .trace_idx = i,
                    .sample_idx = j,
                    .track_x_m = bscan.traces[i].distance_along_track_m,
                    .depth_m = depth,
                    .norm_intensity = norm_v,
                    .est_radius_m = est_r,
                    .polarity = pol,
                    .wgs84 = wgs,
                    .enu = enu,
                    .heading_deg = bscan.traces[i].heading_deg,
                    .used = false
                });
            }
        }
    }

    // Reconstruct 3D Cylindrical Conduits
    if (cfg.extract_conduits) {
        if (n_lines >= 2) {
            // Link 2D apexes across parallel survey lines into 3D conduits
            for (std::size_t l = 0; l + 1 < n_lines; ++l) {
                for (auto& seed : line_apexes[l]) {
                    if (seed.used || seed.norm_intensity < cfg.conduit_threshold) {
                        continue;
                    }
                    std::vector<TargetApex2D*> chain{&seed};
                    seed.used = true;

                    for (std::size_t nl = l + 1; nl < n_lines; ++nl) {
                        const TargetApex2D* prev = chain.back();
                        TargetApex2D* best_next = nullptr;
                        double best_cost = 1e9;

                        for (auto& cand : line_apexes[nl]) {
                            if (cand.used || cand.norm_intensity < cfg.conduit_threshold * 0.80) {
                                continue;
                            }
                            const double dz = std::abs(cand.depth_m - prev->depth_m);
                            const double dx_track = std::abs(cand.track_x_m - prev->track_x_m);
                            if (dz > 0.28 || dx_track > 1.20) {
                                continue;
                            }
                            const double pol_penalty = (cand.polarity == prev->polarity) ? 0.0 : 0.15;
                            const double cost = dx_track + 2.0 * dz + pol_penalty;
                            if (cost < best_cost) {
                                best_cost = cost;
                                best_next = &cand;
                            }
                        }

                        if (best_next != nullptr) {
                            best_next->used = true;
                            chain.push_back(best_next);
                        } else {
                            break;
                        }
                    }

                    if (chain.size() >= 2) {
                        scene::ConduitObject3D cond{};
                        cond.id = scene.conduits.size() + 1;
                        cond.polarity = chain.front()->polarity;
                        cond.label = "Conduit_0" + std::to_string(cond.id) + "_" +
                                     (cond.polarity == PolarityKind::MetallicOrHighPermittivity
                                          ? "Metallic"
                                          : "FiberDuct");

                        double sum_r = 0.0, sum_d = 0.0, max_i = 0.0;
                        for (const auto* node : chain) {
                            cond.centerline_wgs84.push_back(node->wgs84);
                            cond.centerline_utm.push_back(
                                geo::wgs84_to_utm(node->wgs84, scene.origin_utm.zone));
                            cond.centerline_enu.push_back(node->enu);
                            sum_r += node->est_radius_m;
                            sum_d += node->depth_m;
                            max_i = std::max(max_i, node->norm_intensity);
                        }
                        cond.estimated_radius_m = sum_r / static_cast<double>(chain.size());
                        cond.mean_depth_m = sum_d / static_cast<double>(chain.size());
                        cond.peak_intensity = max_i;
                        cond.confidence = std::clamp(
                            0.55 + 0.45 * (static_cast<double>(chain.size()) / static_cast<double>(n_lines)),
                            0.0, 1.0);

                        const ColorRgba col =
                            (cond.polarity == PolarityKind::MetallicOrHighPermittivity)
                                ? ColorRgba{0.94f, 0.26f, 0.22f, 1.0f}   // Crimson red = metallic/power/water
                                : ColorRgba{0.12f, 0.82f, 0.54f, 1.0f};  // Emerald cyan = HDPE fiber duct

                        cond.mesh = build_tube_mesh(
                            cond.label,
                            cond.centerline_enu,
                            cond.estimated_radius_m,
                            cfg.tube_radial_segments,
                            col,
                            cond.polarity == PolarityKind::MetallicOrHighPermittivity ? 0.75f : 0.15f,
                            0.35f);
                        scene.conduits.push_back(std::move(cond));
                    } else {
                        // Release unlinked single node so anomaly stage can capture it
                        seed.used = false;
                    }
                }
            }
        } else {
            // Single B-scan profile: extrude each strong conduit crossing perpendicular to heading
            for (auto& apex : line_apexes.front()) {
                if (apex.norm_intensity < cfg.conduit_threshold) {
                    continue;
                }
                apex.used = true;

                const double p_rad = (apex.heading_deg + 90.0) * (std::numbers::pi / 180.0);
                const double dx = cfg.single_line_half_length_m * std::sin(p_rad);
                const double dy = cfg.single_line_half_length_m * std::cos(p_rad);

                const EnuCoord p0{apex.enu.east_m - dx, apex.enu.north_m - dy, apex.enu.up_m};
                const EnuCoord p1 = apex.enu;
                const EnuCoord p2{apex.enu.east_m + dx, apex.enu.north_m + dy, apex.enu.up_m};

                scene::ConduitObject3D cond{};
                cond.id = scene.conduits.size() + 1;
                cond.polarity = apex.polarity;
                cond.label = "Conduit_0" + std::to_string(cond.id) + "_" +
                             (cond.polarity == PolarityKind::MetallicOrHighPermittivity
                                  ? "Metallic"
                                  : "FiberDuct");
                cond.estimated_radius_m = apex.est_radius_m;
                cond.mean_depth_m = apex.depth_m;
                cond.peak_intensity = apex.norm_intensity;
                cond.confidence = std::clamp(apex.norm_intensity, 0.0, 1.0);

                cond.centerline_enu = {p0, p1, p2};
                for (const auto& ep : cond.centerline_enu) {
                    const Wgs84Coord wp = geo::enu_to_wgs84(ep, scene.origin_wgs84);
                    cond.centerline_wgs84.push_back(wp);
                    cond.centerline_utm.push_back(geo::wgs84_to_utm(wp, scene.origin_utm.zone));
                }

                const ColorRgba col =
                    (cond.polarity == PolarityKind::MetallicOrHighPermittivity)
                        ? ColorRgba{0.94f, 0.26f, 0.22f, 1.0f}
                        : ColorRgba{0.12f, 0.82f, 0.54f, 1.0f};

                cond.mesh = build_tube_mesh(
                    cond.label,
                    cond.centerline_enu,
                    cond.estimated_radius_m,
                    cfg.tube_radial_segments,
                    col,
                    cond.polarity == PolarityKind::MetallicOrHighPermittivity ? 0.75f : 0.15f,
                    0.35f);
                scene.conduits.push_back(std::move(cond));
            }
        }
    }

    // Reconstruct remaining unlinked 3D localized targets as Ellipsoidal Anomaly Objects
    if (cfg.extract_anomalies) {
        for (std::size_t l = 0; l < n_lines; ++l) {
            for (auto& apex : line_apexes[l]) {
                if (apex.used || apex.norm_intensity < cfg.anomaly_threshold) {
                    continue;
                }
                apex.used = true;

                scene::AnomalyObject3D anom{};
                anom.id = scene.anomalies.size() + 1;
                anom.label = "Anomaly_0" + std::to_string(anom.id);
                anom.polarity = apex.polarity;
                anom.center_wgs84 = apex.wgs84;
                anom.center_utm = geo::wgs84_to_utm(apex.wgs84, scene.origin_utm.zone);
                anom.center_enu = apex.enu;
                anom.radii_m = Vec3{
                    std::max(0.10, 2.2 * apex.est_radius_m),
                    std::max(0.10, 2.2 * apex.est_radius_m),
                    std::max(0.07, 1.4 * apex.est_radius_m)
                };
                anom.depth_m = apex.depth_m;
                anom.peak_intensity = apex.norm_intensity;
                anom.mesh = build_ellipsoid_mesh(
                    anom.label,
                    anom.center_enu,
                    anom.radii_m,
                    10,
                    16,
                    ColorRgba{0.96f, 0.56f, 0.12f, 0.90f});

                scene.anomalies.push_back(std::move(anom));
            }
        }
    }

    // Build accompanying georeferenced 3D point cloud
    InterpretationConfig pc_cfg{};
    pc_cfg.mode = ExtractionMode::ThresholdedVolume;
    pc_cfg.min_intensity_threshold = cfg.point_cloud_threshold;
    pc_cfg.min_depth_m = cfg.min_depth_m;
    pc_cfg.max_depth_m = cfg.max_depth_m;
    pc_cfg.forced_utm_zone = scene.origin_utm.zone;
    for (const auto& tb : target_lines) {
        RadarInterpreter::append_bscan_to_3d(scene.point_cloud, tb, pc_cfg);
    }

    scene.primary_processed_bscan = target_lines.front();
    scene.fluctuations = analyze_fluctuations(scene.primary_processed_bscan);

    return scene;
}

}  // namespace gpr3d::dsp

