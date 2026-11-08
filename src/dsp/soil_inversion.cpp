#include "gpr3d/dsp/soil_inversion.hpp"
#include "gpr3d/dsp/fft.hpp"
#include "gpr3d/dsp/signal_processor.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
#include <numeric>
#include <vector>

namespace gpr3d::dsp {

namespace {

constexpr double kFreeSpaceImpedanceOhms = 376.730313668;
constexpr double kDbPerNeper = 8.685889638065036;  // 20 / ln(10)

double sample_linear(const std::vector<double>& s, double f_idx) noexcept {
    if (s.empty() || f_idx < 0.0) {
        return 0.0;
    }
    const auto idx0 = static_cast<std::size_t>(f_idx);
    if (idx0 + 1 >= s.size()) {
        return (idx0 < s.size()) ? s[idx0] : 0.0;
    }
    const double frac = f_idx - static_cast<double>(idx0);
    return (1.0 - frac) * s[idx0] + frac * s[idx0 + 1];
}

double median_of(std::vector<double>& vals) {
    if (vals.empty()) {
        return 0.0;
    }
    const auto mid = vals.begin() + static_cast<std::ptrdiff_t>(vals.size() / 2);
    std::nth_element(vals.begin(), mid, vals.end());
    return *mid;
}

/// Lightweight horizon tracker for standalone `analyze_subsurface` diagnostics.
std::vector<HorizonBoundaryInput> track_diagnostic_horizons(
    const BScan& strat_bscan,
    double min_depth_m,
    double max_depth_m) {
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
        if (d < min_depth_m) j_min = j + 1;
        if (d <= max_depth_m) j_max = j;
    }
    if (j_min + 4 >= j_max) {
        return {};
    }

    // Subsample traces if huge (e.g. 10,000 traces) for rapid diagnostic median profile
    const std::size_t step = std::max<std::size_t>(1, n_traces / 200);
    std::vector<double> med_profile(ns, 0.0);
    std::vector<double> buf;
    buf.reserve(n_traces / step + 2);

    for (std::size_t j = j_min; j <= j_max; ++j) {
        buf.clear();
        for (std::size_t i = 0; i < n_traces; i += step) {
            if (j < strat_bscan.traces[i].envelope.size()) {
                buf.push_back(strat_bscan.traces[i].envelope[j]);
            }
        }
        med_profile[j] = median_of(buf);
    }

    std::vector<double> valid_meds(
        med_profile.begin() + static_cast<std::ptrdiff_t>(j_min),
        med_profile.begin() + static_cast<std::ptrdiff_t>(j_max + 1));
    const double noise_floor = median_of(valid_meds);

    double max_peak = 0.0;
    for (std::size_t j = j_min + 2; j + 2 <= j_max; ++j) {
        if (med_profile[j] > med_profile[j - 1] && med_profile[j] >= med_profile[j + 1]) {
            max_peak = std::max(max_peak, med_profile[j]);
        }
    }
    if (max_peak <= 0.0) {
        return {};
    }

    const double cutoff = std::max(0.22 * max_peak, 3.5 * noise_floor);
    const auto min_sep = static_cast<std::size_t>(
        std::max(6L, std::lround(0.18 / std::max(1e-4, 0.5 * v_m_ns * dt))));

    std::vector<std::size_t> seeds;
    for (std::size_t j = j_min + 2; j + 2 <= j_max; ++j) {
        if (med_profile[j] >= cutoff &&
            med_profile[j] > med_profile[j - 1] &&
            med_profile[j] >= med_profile[j + 1]) {
            seeds.push_back(j);
        }
    }
    std::sort(seeds.begin(), seeds.end(),
              [&](std::size_t a, std::size_t b) { return med_profile[a] > med_profile[b]; });

    std::vector<HorizonBoundaryInput> out;
    std::vector<bool> occupied(ns, false);

    for (std::size_t sj : seeds) {
        if (occupied[sj] || out.size() >= 4) {
            continue;
        }
        // Check lateral coverage across subsampled traces
        std::size_t active = 0;
        std::size_t total = 0;
        for (std::size_t i = 0; i < n_traces; i += step) {
            ++total;
            double loc_max = 0.0;
            const std::size_t a = (sj > 3) ? (sj - 3) : j_min;
            const std::size_t b = std::min(j_max, sj + 3);
            for (std::size_t j = a; j <= b; ++j) {
                loc_max = std::max(loc_max, strat_bscan.traces[i].envelope[j]);
            }
            if (loc_max >= 0.50 * med_profile[sj]) {
                ++active;
            }
        }
        if (total == 0 || static_cast<double>(active) / static_cast<double>(total) < 0.65) {
            continue;
        }

        for (std::size_t j = (sj > min_sep ? sj - min_sep : 0);
             j <= std::min(ns - 1, sj + min_sep); ++j) {
            occupied[j] = true;
        }

        std::vector<std::size_t> idx_vec(n_traces, sj);
        double sum_d = 0.0;
        for (std::size_t i = 0; i < n_traces; ++i) {
            std::size_t bj = sj;
            const std::size_t a = (sj > 3) ? (sj - 3) : j_min;
            const std::size_t b = std::min(j_max, sj + 3);
            for (std::size_t j = a; j <= b; ++j) {
                if (strat_bscan.traces[i].envelope[j] > strat_bscan.traces[i].envelope[bj]) {
                    bj = j;
                }
            }
            idx_vec[i] = bj;
            sum_d += 0.5 * v_m_ns * static_cast<double>(bj) * dt;
        }

        out.push_back(HorizonBoundaryInput{
            .sample_indices = std::move(idx_vec),
            .mean_depth_m = sum_d / static_cast<double>(n_traces)
        });
    }

    std::sort(out.begin(), out.end(),
              [](const HorizonBoundaryInput& a, const HorizonBoundaryInput& b) {
                  return a.mean_depth_m < b.mean_depth_m;
              });
    return out;
}

}  // namespace

double SoilInverter::invert_fresnel_permittivity(
    double eps_r_upper,
    double reflection_coeff_R) noexcept {
    const double eps_top = std::clamp(eps_r_upper, 1.0, 84.0);
    const double r = std::clamp(reflection_coeff_R, -0.88, 0.88);
    const double ratio = (1.0 - r) / (1.0 + r);
    return std::clamp(eps_top * ratio * ratio, 1.0, 84.0);
}

scene::HyperbolaFitResult SoilInverter::fit_hyperbola_velocity(
    const BScan& unmigrated_bscan,
    std::size_t apex_trace_idx,
    std::size_t apex_sample_idx,
    double half_aperture_m,
    double target_radius_m) {
    scene::HyperbolaFitResult res{};
    res.trace_idx = apex_trace_idx;
    res.sample_idx = apex_sample_idx;

    const std::size_t n_tr = unmigrated_bscan.num_traces();
    const std::size_t ns = unmigrated_bscan.num_samples();
    const double dt = unmigrated_bscan.metadata.sample_interval_ns;

    if (n_tr < 9 || ns < 16 || dt <= 0.0 ||
        apex_trace_idx >= n_tr || apex_sample_idx < 4 || apex_sample_idx + 5 >= ns) {
        return res;
    }

    // Snap apex_sample_idx within +/- 3 samples to strongest signed crest/trough (avoids 90-deg wavelet zero-crossing)
    {
        const auto& apex_s = unmigrated_bscan.traces[apex_trace_idx].samples;
        const std::size_t ja = (apex_sample_idx > 3) ? (apex_sample_idx - 3) : 4;
        const std::size_t jb = std::min(ns - 5, apex_sample_idx + 3);
        std::size_t best_j = apex_sample_idx;
        for (std::size_t j = ja; j <= jb; ++j) {
            if (std::abs(apex_s[j]) > std::abs(apex_s[best_j])) {
                best_j = j;
            }
        }
        apex_sample_idx = best_j;
        res.sample_idx = apex_sample_idx;
    }

    const double x0 = unmigrated_bscan.traces[apex_trace_idx].distance_along_track_m;
    const double t0 = static_cast<double>(apex_sample_idx) * dt;
    res.x0_m = x0;
    res.t0_ns = t0;

    // Adapt maximum lateral coherence aperture to antenna frequency & target depth
    const double nom_eps = std::clamp(unmigrated_bscan.metadata.relative_permittivity, 2.0, 36.0);
    const double nom_z0 = 0.5 * (kSpeedOfLightMPerNs / std::sqrt(nom_eps)) * t0;
    const double hdr_f_mhz = unmigrated_bscan.metadata.antenna_freq_mhz;
    const double eff_half_aperture_m =
        (hdr_f_mhz >= 1000.0)
            ? std::clamp(1.15 * nom_z0 + 0.04, 0.12, std::min(0.45, half_aperture_m))
            : std::clamp(1.50 * nom_z0 + 0.12, 0.20, half_aperture_m);

    // Gather symmetric aperture traces around x0
    std::vector<std::size_t> cand_traces;
    std::size_t left_count = 0;
    std::size_t right_count = 0;
    for (std::size_t i = 0; i < n_tr; ++i) {
        const double dx = unmigrated_bscan.traces[i].distance_along_track_m - x0;
        if (std::abs(dx) <= eff_half_aperture_m) {
            cand_traces.push_back(i);
            if (dx < -1e-4) ++left_count;
            if (dx > 1e-4) ++right_count;
        }
    }
    if (left_count < 3 || right_count < 3 || cand_traces.size() < 7) {
        return res;
    }

    // Subsample if ultra-dense trace spacing so semblance stays fast and balanced
    std::vector<std::size_t> active_traces;
    const std::size_t tr_stride = std::max<std::size_t>(1, cand_traces.size() / 48);
    for (std::size_t k = 0; k < cand_traces.size(); k += tr_stride) {
        active_traces.push_back(cand_traces[k]);
    }
    if (active_traces.size() < 7) {
        active_traces = cand_traces;
    }

    // Half-wavelet coherence window M samples
    const double nom_f_ghz = std::clamp(unmigrated_bscan.metadata.antenna_freq_mhz * 1e-3, 0.08, 2.5);
    const int half_win = std::clamp(static_cast<int>(std::lround(0.30 / (nom_f_ghz * dt))), 1, 5);
    const double radius = std::max(0.0, target_radius_m);

    // Velocity search grid: v in [0.050, 0.200] m/ns (eps_r in [2.25, 36.0])
    constexpr double v_min = 0.050;
    constexpr double v_max = 0.200;
    constexpr int n_grid = 76;
    const double dv = (v_max - v_min) / static_cast<double>(n_grid - 1);

    std::array<double, n_grid> semblances{};
    int valid_grid_pts = 0;

    for (int g = 0; g < n_grid; ++g) {
        const double v = v_min + static_cast<double>(g) * dv;
        const double z0 = 0.5 * v * t0;

        double num_sum = 0.0;
        double den_sum = 0.0;
        int used_k = 0;
        double max_moveout_ns = 0.0;

        // First verify how many traces fit inside sample bounds at this trial velocity
        std::vector<double> trace_fidx;
        trace_fidx.reserve(active_traces.size());
        for (std::size_t ti : active_traces) {
            const double dx = unmigrated_bscan.traces[ti].distance_along_track_m - x0;
            const double dist = std::hypot(dx, z0 + radius) - radius;
            const double t_hyp = (2.0 * std::max(0.0, dist)) / v;
            const double f_idx = t_hyp / dt;
            if (f_idx + static_cast<double>(half_win) < static_cast<double>(ns - 1) &&
                f_idx >= static_cast<double>(half_win)) {
                trace_fidx.push_back(f_idx);
                max_moveout_ns = std::max(max_moveout_ns, t_hyp - t0);
                ++used_k;
            } else {
                trace_fidx.push_back(-1.0);
            }
        }

        if (used_k < 7 || max_moveout_ns < 2.0 * dt) {
            semblances[static_cast<std::size_t>(g)] = 0.0;
            continue;
        }

        for (int w = -half_win; w <= half_win; ++w) {
            double stack_amp = 0.0;
            double energy_amp = 0.0;
            for (std::size_t m = 0; m < active_traces.size(); ++m) {
                if (trace_fidx[m] < 0.0) continue;
                const double s = sample_linear(
                    unmigrated_bscan.traces[active_traces[m]].samples,
                    trace_fidx[m] + static_cast<double>(w));
                stack_amp += s;
                energy_amp += s * s;
            }
            num_sum += stack_amp * stack_amp;
            den_sum += energy_amp;
        }

        if (den_sum > 1e-18) {
            semblances[static_cast<std::size_t>(g)] =
                num_sum / (static_cast<double>(used_k) * den_sum);
            ++valid_grid_pts;
        }
    }

    if (valid_grid_pts < 8) {
        return res;
    }

    int best_g = 0;
    for (int g = 1; g < n_grid; ++g) {
        if (semblances[static_cast<std::size_t>(g)] > semblances[static_cast<std::size_t>(best_g)]) {
            best_g = g;
        }
    }

    const double peak_s = semblances[static_cast<std::size_t>(best_g)];
    if (peak_s < 0.28) {
        return res;
    }

    // Reject flat non-hyperbolic events whose semblance increases monotonically to v_max
    if (best_g >= n_grid - 2 && semblances[n_grid - 1] >= semblances[n_grid - 4]) {
        return res;
    }

    // Sub-grid 3-point parabolic refinement around best_g
    double v_star = v_min + static_cast<double>(best_g) * dv;
    if (best_g > 0 && best_g + 1 < n_grid) {
        const double s_l = semblances[static_cast<std::size_t>(best_g - 1)];
        const double s_c = peak_s;
        const double s_r = semblances[static_cast<std::size_t>(best_g + 1)];
        const double denom = s_l - 2.0 * s_c + s_r;
        if (std::abs(denom) > 1e-9 && denom < 0.0) {
            const double delta = std::clamp(0.5 * (s_l - s_r) / denom, -0.5, 0.5);
            v_star = v_min + (static_cast<double>(best_g) + delta) * dv;
        }
    }

    // Prominence relative to median semblance across grid
    std::vector<double> s_copy(semblances.begin(), semblances.end());
    const double med_s = median_of(s_copy);
    const double prominence = std::max(0.0, peak_s - med_s);
    if (prominence < 0.05) {
        return res;
    }

    res.valid = true;
    res.velocity_m_ns = v_star;
    res.estimated_epsr = std::clamp(
        (kSpeedOfLightMPerNs / v_star) * (kSpeedOfLightMPerNs / v_star), 1.5, 45.0);
    res.depth_m = 0.5 * v_star * t0;
    res.semblance = peak_s;
    res.confidence = std::clamp(peak_s * std::min(1.0, prominence / 0.16), 0.15, 0.98);
    return res;
}

std::vector<scene::HyperbolaFitResult> SoilInverter::scan_and_fit_hyperbolas(
    const BScan& unmigrated_bscan,
    double min_depth_m,
    double max_depth_m) {
    if (unmigrated_bscan.empty() || unmigrated_bscan.num_samples() < 16) {
        return {};
    }

    BScan work = unmigrated_bscan;
    if (work.traces.front().envelope.empty()) {
        SignalProcessor::compute_envelopes(work);
    }

    const std::size_t n_tr = work.num_traces();
    const std::size_t ns = work.num_samples();
    const double dt = work.metadata.sample_interval_ns;
    const double nom_eps = std::max(1.0, work.metadata.relative_permittivity);
    const double nom_v = kSpeedOfLightMPerNs / std::sqrt(nom_eps);

    // Leave 8% bottom sample margin so hyperbola flanks have room to curve downward
    const std::size_t safe_max_j = static_cast<std::size_t>(0.92 * static_cast<double>(ns));
    std::size_t j_min = 4;
    std::size_t j_max = std::min(ns - 5, std::max<std::size_t>(8, safe_max_j));
    for (std::size_t j = 0; j < ns; ++j) {
        const double d = 0.5 * nom_v * static_cast<double>(j) * dt;
        if (d < min_depth_m) j_min = std::max<std::size_t>(4, j + 1);
        if (d <= max_depth_m && j <= safe_max_j) j_max = j;
    }
    if (j_min + 4 >= j_max) {
        return {};
    }

    double max_env = 0.0;
    for (std::size_t i = 0; i < n_tr; ++i) {
        for (std::size_t j = j_min; j <= j_max; ++j) {
            max_env = std::max(max_env, work.traces[i].envelope[j]);
        }
    }
    if (max_env <= 0.0) {
        return {};
    }

    const auto nms_i = static_cast<std::size_t>(
        std::max(4L, std::lround(0.45 / std::max(0.005, work.metadata.trace_spacing_m))));
    const auto nms_j = static_cast<std::size_t>(
        std::max(5L, std::lround(0.18 / std::max(1e-4, 0.5 * nom_v * dt))));

    struct CandPeak {
        std::size_t i{};
        std::size_t j{};
        double env{};
    };
    std::vector<CandPeak> cands;

    for (std::size_t i = 3; i + 3 < n_tr; ++i) {
        for (std::size_t j = j_min; j <= j_max; ++j) {
            const double val = work.traces[i].envelope[j];
            if (val < 0.28 * max_env) continue;

            bool is_max = true;
            const std::size_t ia = (i > nms_i) ? (i - nms_i) : 0;
            const std::size_t ib = std::min(n_tr - 1, i + nms_i);
            const std::size_t ja = (j > nms_j) ? (j - nms_j) : j_min;
            const std::size_t jb = std::min(j_max, j + nms_j);

            for (std::size_t ii = ia; ii <= ib && is_max; ++ii) {
                for (std::size_t jj = ja; jj <= jb; ++jj) {
                    if (ii == i && jj == j) continue;
                    if (work.traces[ii].envelope[jj] > val) {
                        is_max = false;
                        break;
                    }
                }
            }
            if (is_max) {
                cands.push_back({i, j, val});
            }
        }
    }

    std::sort(cands.begin(), cands.end(),
              [](const CandPeak& a, const CandPeak& b) { return a.env > b.env; });
    if (cands.size() > 14) {
        cands.resize(14);
    }

    std::vector<scene::HyperbolaFitResult> fits;
    for (const auto& c : cands) {
        auto fit = fit_hyperbola_velocity(work, c.i, c.j, 1.10, 0.04);
        if (fit.valid && fit.semblance >= 0.32 && fit.confidence >= 0.22) {
            fits.push_back(fit);
        }
    }
    return fits;
}

scene::AttenuationEstimate SoilInverter::estimate_trace_attenuation(
    const BScan& conditioned_no_gain_bscan,
    double eps_r,
    double min_depth_m,
    double max_depth_m) {
    scene::AttenuationEstimate out{};
    const double clean_eps = std::clamp(eps_r, 1.0, 84.0);
    const double v_m_ns = kSpeedOfLightMPerNs / std::sqrt(clean_eps);
    const std::size_t n_tr = conditioned_no_gain_bscan.num_traces();
    const std::size_t ns = conditioned_no_gain_bscan.num_samples();
    const double dt = conditioned_no_gain_bscan.metadata.sample_interval_ns;

    if (n_tr == 0 || ns < 16 || dt <= 0.0) {
        return out;
    }

    std::size_t j_min = 2;
    std::size_t j_max = ns - 2;
    for (std::size_t j = 0; j < ns; ++j) {
        const double d = 0.5 * v_m_ns * static_cast<double>(j) * dt;
        if (d < min_depth_m) j_min = j + 1;
        if (d <= max_depth_m) j_max = j;
    }
    if (j_min + 8 >= j_max) {
        j_min = 2;
        j_max = ns - 2;
    }

    const std::size_t tr_step = std::max<std::size_t>(1, n_tr / 100);
    const double fs_mhz = 1000.0 / dt;

    // 1. Measure ensemble spectral centroid & Quan-Harris shallow-to-deep centroid shift
    const std::size_t span_j = j_max - j_min + 1;
    const std::size_t win_len = std::max<std::size_t>(16, span_j / 2);
    const std::size_t n_fft = next_power_of_two(win_len);
    const std::size_t n_pos = n_fft / 2 + 1;

    const std::size_t j_sh0 = j_min;
    const std::size_t j_sh1 = std::min(j_max, j_sh0 + win_len - 1);
    const std::size_t j_dp1 = j_max;
    const std::size_t j_dp0 = (j_dp1 > j_min + win_len - 1) ? (j_dp1 - win_len + 1) : j_min;

    std::vector<double> psd_sh(n_pos, 0.0);
    std::vector<double> psd_dp(n_pos, 0.0);
    std::vector<std::complex<double>> fft_buf(n_fft);

    for (std::size_t i = 0; i < n_tr; i += tr_step) {
        const auto& s = conditioned_no_gain_bscan.traces[i].samples;

        // Shallow window with Hann taper
        std::fill(fft_buf.begin(), fft_buf.end(), std::complex<double>{0.0, 0.0});
        const std::size_t len_s = j_sh1 - j_sh0 + 1;
        for (std::size_t k = 0; k < len_s; ++k) {
            const double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * static_cast<double>(k) /
                                                   static_cast<double>(std::max<std::size_t>(1, len_s - 1))));
            fft_buf[k] = {s[j_sh0 + k] * w, 0.0};
        }
        const auto sp_s = fft_forward(fft_buf);
        for (std::size_t b = 1; b < n_pos; ++b) psd_sh[b] += std::norm(sp_s[b]);

        // Deep window with Hann taper
        std::fill(fft_buf.begin(), fft_buf.end(), std::complex<double>{0.0, 0.0});
        const std::size_t len_d = j_dp1 - j_dp0 + 1;
        for (std::size_t b_k = 0; b_k < len_d; ++b_k) {
            const double w = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * static_cast<double>(b_k) /
                                                   static_cast<double>(std::max<std::size_t>(1, len_d - 1))));
            fft_buf[b_k] = {s[j_dp0 + b_k] * w, 0.0};
        }
        const auto sp_d = fft_forward(fft_buf);
        for (std::size_t b = 1; b < n_pos; ++b) psd_dp[b] += std::norm(sp_d[b]);
    }

    auto centroid_and_var = [&](const std::vector<double>& psd) -> std::pair<double, double> {
        double sum_p = 1e-18;
        double sum_fp = 0.0;
        // Restrict to plausible GPR band <= 3500 MHz
        for (std::size_t b = 1; b < n_pos; ++b) {
            const double f_mhz = (static_cast<double>(b) * fs_mhz) / static_cast<double>(n_fft);
            if (f_mhz > 3500.0) break;
            sum_p += psd[b];
            sum_fp += f_mhz * psd[b];
        }
        const double fc = sum_fp / sum_p;
        double sum_v = 0.0;
        for (std::size_t b = 1; b < n_pos; ++b) {
            const double f_mhz = (static_cast<double>(b) * fs_mhz) / static_cast<double>(n_fft);
            if (f_mhz > 3500.0) break;
            const double df = f_mhz - fc;
            sum_v += df * df * psd[b];
        }
        return {fc, std::max(1000.0, sum_v / sum_p)};
    };

    const auto [fc_sh, var_sh] = centroid_and_var(psd_sh);
    const auto [fc_dp, var_dp] = centroid_and_var(psd_dp);
    const double nom_f = conditioned_no_gain_bscan.metadata.antenna_freq_mhz;
    const double eff_freq_mhz = (nom_f >= 50.0 && nom_f <= 3000.0)
                                    ? (0.5 * nom_f + 0.5 * fc_sh)
                                    : std::clamp(fc_sh, 80.0, 2500.0);

    const double z_sh_mid = 0.25 * v_m_ns * static_cast<double>(j_sh0 + j_sh1) * dt;
    const double z_dp_mid = 0.25 * v_m_ns * static_cast<double>(j_dp0 + j_dp1) * dt;
    const double dz_mid = std::max(0.15, z_dp_mid - z_sh_mid);
    const double dfc_dz = (fc_dp - fc_sh) / dz_mid;  // Negative when high frequencies attenuate

    // Quan & Harris (1997): alpha(f) = alpha_0 * f, where alpha_0 = -(dfc / dL) / var_f, L = 2*z
    const double var_avg_mhz2 = 0.5 * (var_sh + var_dp);
    const double alpha_spec_np_m = std::max(0.0, (-dfc_dz / (2.0 * var_avg_mhz2)) * eff_freq_mhz);

    // 2. Median Hilbert envelope decay with geometric spreading compensation
    std::vector<double> med_env(ns, 0.0);
    std::vector<std::vector<double>> sub_envs;
    sub_envs.reserve(n_tr / tr_step + 2);
    for (std::size_t i = 0; i < n_tr; i += tr_step) {
        const auto& tr = conditioned_no_gain_bscan.traces[i];
        if (!tr.envelope.empty() && tr.envelope.size() == ns) {
            sub_envs.push_back(tr.envelope);
        } else {
            sub_envs.push_back(hilbert_envelope(tr.samples));
        }
    }

    std::vector<double> col_vals;
    col_vals.reserve(sub_envs.size());
    for (std::size_t j = j_min; j <= j_max; ++j) {
        col_vals.clear();
        for (const auto& ev : sub_envs) {
            col_vals.push_back(ev[j]);
        }
        med_env[j] = median_of(col_vals);
    }

    // Estimate late-time noise floor from bottom 15% of window
    std::vector<double> tail_vals;
    const std::size_t j_tail = j_min + static_cast<std::size_t>(0.85 * static_cast<double>(j_max - j_min));
    for (std::size_t j = j_tail; j <= j_max; ++j) {
        tail_vals.push_back(med_env[j]);
    }
    const double noise_floor = std::max(1e-12, median_of(tail_vals));

    // Linear regression of ln(E(z) * (2z)) = C - alpha_np_m * (2z)
    double sum_L = 0.0, sum_y = 0.0, sum_LL = 0.0, sum_Ly = 0.0;
    int n_pts = 0;
    for (std::size_t j = j_min; j <= j_max; ++j) {
        if (med_env[j] <= 1.8 * noise_floor) continue;
        const double z = 0.5 * v_m_ns * static_cast<double>(j) * dt;
        const double L = std::max(0.12, 2.0 * z);  // Two-way path (m)
        const double y = std::log(med_env[j] * std::sqrt(L));  // Cylindrical/planar spreading compensation
        sum_L += L;
        sum_y += y;
        sum_LL += L * L;
        sum_Ly += L * y;
        ++n_pts;
    }

    double alpha_amp_np_m = -1.0;
    if (n_pts >= 8) {
        const double nd = static_cast<double>(n_pts);
        const double den = nd * sum_LL - sum_L * sum_L;
        if (std::abs(den) > 1e-9) {
            const double slope = (nd * sum_Ly - sum_L * sum_y) / den;
            alpha_amp_np_m = -slope;  // Positive if amplitude decays with depth
        }
    }

    double final_alpha_np_m = 0.15;
    if (alpha_amp_np_m > 0.01) {
        out.hardware_gain_detected = false;
        final_alpha_np_m = (alpha_spec_np_m > 0.01)
                               ? (0.72 * alpha_amp_np_m + 0.28 * alpha_spec_np_m)
                               : alpha_amp_np_m;
    } else {
        // Hardware range-gain was already baked into raw data; rely on spectral shift + dielectric baseline
        out.hardware_gain_detected = true;
        const double baseline_np = 0.03 * std::sqrt(clean_eps);
        final_alpha_np_m = std::max(baseline_np, alpha_spec_np_m);
    }
    final_alpha_np_m = std::clamp(final_alpha_np_m, 0.015, 12.0);

    out.attenuation_np_per_m = final_alpha_np_m;
    out.attenuation_db_per_m = final_alpha_np_m * kDbPerNeper;
    out.effective_freq_mhz = eff_freq_mhz;
    out.centroid_shift_mhz_per_m = dfc_dz;
    out.conductivity_ms_m =
        1000.0 * (2.0 * std::sqrt(clean_eps) / kFreeSpaceImpedanceOhms) * final_alpha_np_m;
    out.loss_tangent =
        (299.792458 * final_alpha_np_m) /
        (std::numbers::pi * std::max(10.0, eff_freq_mhz) * std::sqrt(clean_eps));

    return out;
}

std::vector<scene::LithologicalLayer> SoilInverter::invert_stratigraphic_layers(
    const BScan& conditioned_no_gain_bscan,
    std::span<const HorizonBoundaryInput> horizon_boundaries,
    double surface_epsr,
    const scene::AttenuationEstimate& bulk_atten,
    double max_profile_depth_m) {
    std::vector<scene::LithologicalLayer> layers;
    const double clean_surf_eps = std::clamp(surface_epsr, 1.5, 80.0);
    const double max_d = std::max(0.25, max_profile_depth_m);

    const std::size_t n_tr = conditioned_no_gain_bscan.num_traces();
    const std::size_t ns = conditioned_no_gain_bscan.num_samples();
    const std::size_t tr_step = std::max<std::size_t>(1, n_tr / 120);

    // Measure median signed peak amplitude of surface arrival near t = 0 as Fresnel polarity & scale reference
    double surf_signed_ref = 1.0;
    if (n_tr > 0 && ns > 6) {
        const std::size_t surf_win = std::min<std::size_t>(ns - 1, 24);
        std::vector<double> surf_peaks;
        surf_peaks.reserve(n_tr / tr_step + 2);
        for (std::size_t i = 0; i < n_tr; i += tr_step) {
            double best_s = 0.0;
            for (std::size_t j = 0; j <= surf_win; ++j) {
                const double s = conditioned_no_gain_bscan.traces[i].samples[j];
                if (std::abs(s) > std::abs(best_s)) {
                    best_s = s;
                }
            }
            surf_peaks.push_back(best_s);
        }
        const double med_s = median_of(surf_peaks);
        if (std::abs(med_s) > 1e-12) {
            surf_signed_ref = med_s;
        }
    }

    // Air -> Layer 1 Fresnel coefficient R0 = (1 - sqrt(eps_1)) / (1 + sqrt(eps_1)) < 0
    const double r0_fresnel = (1.0 - std::sqrt(clean_surf_eps)) / (1.0 + std::sqrt(clean_surf_eps));
    const double abs_r0 = std::max(0.22, std::abs(r0_fresnel));
    const double surf_sign = (surf_signed_ref >= 0.0) ? 1.0 : -1.0;

    // Sort valid horizon boundaries strictly below direct-wave ringdown (>= 0.20 m) and above bottom margin
    const double min_hz_depth = std::clamp(
        1.6 * (kSpeedOfLightMPerNs / std::sqrt(clean_surf_eps)) *
            (1000.0 / std::max(150.0, bulk_atten.effective_freq_mhz)),
        0.18,
        0.30);
    std::vector<const HorizonBoundaryInput*> valid_hz;
    for (const auto& hz : horizon_boundaries) {
        if (hz.mean_depth_m >= min_hz_depth && hz.mean_depth_m < max_d - 0.08) {
            valid_hz.push_back(&hz);
        }
    }
    std::sort(valid_hz.begin(), valid_hz.end(),
              [](const HorizonBoundaryInput* a, const HorizonBoundaryInput* b) {
                  return a->mean_depth_m < b->mean_depth_m;
              });

    const std::size_t num_layers = valid_hz.size() + 1;
    layers.reserve(num_layers);

    double current_eps = clean_surf_eps;
    double cumulative_trans = 1.0;  // Two-way transmission product prod (1 - R_m^2)
    double cumulative_tau_np = 0.0; // Cumulative one-way attenuation optical depth

    for (std::size_t k = 0; k < num_layers; ++k) {
        const double z_top = (k == 0) ? 0.0 : valid_hz[k - 1]->mean_depth_m;
        const double z_bot = (k < valid_hz.size()) ? valid_hz[k]->mean_depth_m : max_d;
        const double thick = std::max(0.02, z_bot - z_top);

        double r_top = (k == 0) ? r0_fresnel : 0.0;
        if (k > 0 && n_tr > 0 && ns > 6) {
            const auto& hz = *valid_hz[k - 1];
            std::vector<double> hz_amps;
            hz_amps.reserve(n_tr / tr_step + 2);

            for (std::size_t i = 0; i < n_tr; i += tr_step) {
                const std::size_t jc = (i < hz.sample_indices.size())
                                           ? std::min(ns - 1, hz.sample_indices[i])
                                           : std::min(ns - 1, static_cast<std::size_t>(ns / 2));
                const std::size_t a = (jc > 3) ? (jc - 3) : 0;
                const std::size_t b = std::min(ns - 1, jc + 3);
                double best_s = 0.0;
                for (std::size_t j = a; j <= b; ++j) {
                    const double s = conditioned_no_gain_bscan.traces[i].samples[j];
                    if (std::abs(s) > std::abs(best_s)) {
                        best_s = s;
                    }
                }
                hz_amps.push_back(best_s);
            }

            const double med_hz_amp = median_of(hz_amps);
            // Compensate two-way attenuation & transmission losses ONLY if field hardware gain wasn't already applied
            const double atten_comp = bulk_atten.hardware_gain_detected
                                          ? 1.0
                                          : std::min(5.0, std::exp(2.0 * cumulative_tau_np));
            const double spread_comp = bulk_atten.hardware_gain_detected
                                           ? 1.0
                                           : std::sqrt(std::max(1.0, 1.0 + 2.0 * z_top));
            const double raw_mag =
                (std::abs(med_hz_amp) * atten_comp * spread_comp * abs_r0) /
                (std::max(1e-12, std::abs(surf_signed_ref)) * std::max(0.30, cumulative_trans));
            const double r_mag = std::clamp(0.34 * std::tanh(raw_mag / 0.34), 0.04, 0.34);

            // Same waveform polarity as air->ground reflection => permittivity increases downward (Fresnel R < 0)
            const double hz_sign = (med_hz_amp >= 0.0) ? 1.0 : -1.0;
            r_top = (hz_sign == surf_sign) ? -r_mag : +r_mag;

            const double eps_floor = (clean_surf_eps < 3.0) ? 1.5 : 3.0;
            const double eps_ceil = (clean_surf_eps > 45.0) ? 84.0 : 42.0;
            current_eps = std::clamp(invert_fresnel_permittivity(current_eps, r_top), eps_floor, eps_ceil);
            cumulative_trans *= (1.0 - r_top * r_top);
        }

        // Layer-specific attenuation (blend local window estimate with bulk estimate for thin layers)
        scene::AttenuationEstimate layer_att = bulk_atten;
        if (thick >= 0.30 && n_tr > 0) {
            const auto local_att = estimate_trace_attenuation(
                conditioned_no_gain_bscan, current_eps, std::max(0.04, z_top), z_bot);
            layer_att.attenuation_np_per_m =
                0.65 * local_att.attenuation_np_per_m + 0.35 * bulk_atten.attenuation_np_per_m;
        } else {
            // Adjust bulk attenuation slightly according to dielectric contrast of layer
            const double eps_scale = std::sqrt(current_eps / clean_surf_eps);
            layer_att.attenuation_np_per_m = std::clamp(bulk_atten.attenuation_np_per_m * eps_scale, 0.015, 12.0);
        }

        layer_att.attenuation_db_per_m = layer_att.attenuation_np_per_m * kDbPerNeper;
        layer_att.conductivity_ms_m =
            1000.0 * (2.0 * std::sqrt(current_eps) / kFreeSpaceImpedanceOhms) * layer_att.attenuation_np_per_m;
        layer_att.loss_tangent =
            (299.792458 * layer_att.attenuation_np_per_m) /
            (std::numbers::pi * std::max(10.0, bulk_atten.effective_freq_mhz) * std::sqrt(current_eps));

        cumulative_tau_np += layer_att.attenuation_np_per_m * thick;

        auto ranked = geo::MaterialDatabase::rank_soil_materials(
            current_eps,
            layer_att.conductivity_ms_m,
            layer_att.attenuation_db_per_m,
            layer_att.loss_tangent,
            z_top,
            3);

        scene::LithologicalLayer lyr{};
        lyr.layer_index = k + 1;
        lyr.depth_top_m = z_top;
        lyr.depth_bottom_m = z_bot;
        lyr.thickness_m = thick;
        lyr.reflection_coeff_top = r_top;
        lyr.estimated_epsr = current_eps;
        lyr.velocity_m_ns = kSpeedOfLightMPerNs / std::sqrt(current_eps);
        lyr.attenuation_db_m = layer_att.attenuation_db_per_m;
        lyr.conductivity_ms_m = layer_att.conductivity_ms_m;
        lyr.loss_tangent = layer_att.loss_tangent;
        if (!ranked.empty()) {
            lyr.primary_material = ranked.front();
            lyr.candidates = std::move(ranked);
        } else {
            lyr.primary_material = geo::MaterialDatabase::classify_soil_layer(
                current_eps, layer_att.conductivity_ms_m, layer_att.attenuation_db_per_m, layer_att.loss_tangent, z_top);
        }

        layers.push_back(std::move(lyr));
    }

    return layers;
}

scene::SubsurfaceMaterialReport SoilInverter::analyze_subsurface(
    const BScan& raw_bscan,
    double min_depth_m,
    double max_depth_m) {
    scene::SubsurfaceMaterialReport rep{};
    if (raw_bscan.empty() || raw_bscan.num_samples() == 0) {
        return rep;
    }

    rep.nominal_header_epsr = std::max(1.0, raw_bscan.metadata.relative_permittivity);

    // 1. Condition traces WITHOUT SEC gain or background removal to preserve physical amplitudes & decay
    BScan clean_no_gain = raw_bscan;
    ProcessingConfig no_gain_cfg{};
    no_gain_cfg.enable_dewow = true;
    no_gain_cfg.enable_time_zero = true;
    no_gain_cfg.enable_bandpass = true;
    no_gain_cfg.enable_background_removal = false;
    no_gain_cfg.enable_sec_gain = false;
    no_gain_cfg.enable_migration = false;
    no_gain_cfg.compute_envelope = true;
    SignalProcessor::process(clean_no_gain, no_gain_cfg);

    // 2. Remove background on a branch to expose unmigrated diffraction hyperbolas (mild SEC gain avoids deep noise blowup)
    BScan unmigrated_bg = clean_no_gain;
    SignalProcessor::remove_background(unmigrated_bg, 0);
    SignalProcessor::apply_sec_gain(unmigrated_bg, 0.04, 0.015, 8.0);
    SignalProcessor::compute_envelopes(unmigrated_bg);

    rep.hyperbola_fits = scan_and_fit_hyperbolas(unmigrated_bg, std::max(0.06, min_depth_m * 0.7), max_depth_m);

    // Fuse fitted hyperbola velocities with header permittivity weighted by fit confidence
    double sum_w = 0.65;  // Gentle regularizing prior weight on header permittivity
    double sum_eps = 0.65 * rep.nominal_header_epsr;
    double sum_semb = 0.0;
    for (const auto& f : rep.hyperbola_fits) {
        sum_w += f.confidence;
        sum_eps += f.confidence * f.estimated_epsr;
        sum_semb += f.semblance;
    }
    rep.inverted_bulk_epsr = std::clamp(sum_eps / sum_w, 1.5, 45.0);
    rep.inverted_bulk_velocity_m_ns = kSpeedOfLightMPerNs / std::sqrt(rep.inverted_bulk_epsr);
    rep.mean_hyperbola_semblance = rep.hyperbola_fits.empty()
                                       ? 0.0
                                       : (sum_semb / static_cast<double>(rep.hyperbola_fits.size()));

    const double max_win_depth =
        0.5 * rep.inverted_bulk_velocity_m_ns *
        static_cast<double>(clean_no_gain.num_samples()) * clean_no_gain.metadata.sample_interval_ns;
    const double eff_max_depth = std::min(max_depth_m, std::max(0.30, max_win_depth));

    // 3. Frequency-dependent attenuation & conductivity on un-gained traces
    rep.bulk_attenuation = estimate_trace_attenuation(
        clean_no_gain, rep.inverted_bulk_epsr, min_depth_m, eff_max_depth);

    // 4. Track stratigraphic horizons on mild SEC-gained branch & run Fresnel multilayer inversion
    BScan strat_branch = clean_no_gain;
    SignalProcessor::apply_sec_gain(strat_branch, 0.06, 0.03, 30.0);
    SignalProcessor::compute_envelopes(strat_branch);
    const auto hz_bounds = track_diagnostic_horizons(strat_branch, min_depth_m, eff_max_depth);

    rep.strata = invert_stratigraphic_layers(
        clean_no_gain, hz_bounds, rep.inverted_bulk_epsr, rep.bulk_attenuation, eff_max_depth);

    // Select dominant subsurface soil (thickest layer)
    if (!rep.strata.empty()) {
        const auto best_it = std::max_element(
            rep.strata.begin(), rep.strata.end(),
            [](const scene::LithologicalLayer& a, const scene::LithologicalLayer& b) {
                return a.thickness_m < b.thickness_m;
            });
        rep.dominant_soil = best_it->primary_material;
    } else {
        rep.dominant_soil = geo::MaterialDatabase::classify_soil_layer(
            rep.inverted_bulk_epsr,
            rep.bulk_attenuation.conductivity_ms_m,
            rep.bulk_attenuation.attenuation_db_per_m,
            rep.bulk_attenuation.loss_tangent,
            0.20);
    }

    return rep;
}

}  // namespace gpr3d::dsp
