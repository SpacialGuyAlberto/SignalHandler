#include "gpr3d/dsp/signal_processor.hpp"
#include "gpr3d/dsp/fft.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace gpr3d::dsp {

void SignalProcessor::process(BScan& bscan, const ProcessingConfig& cfg) {
    if (bscan.empty()) {
        return;
    }

    // 1. Dewow first so DC offset does not bias first-break detection
    if (cfg.enable_dewow) {
        apply_dewow(bscan, cfg.dewow_window_ns);
    }

    // 2. Align ground surface arrival to t = 0 ns
    if (cfg.enable_time_zero) {
        apply_time_zero(bscan, cfg.fixed_time_zero_ns, cfg.first_break_threshold);
    }

    // 3. Zero-phase frequency bandpass around antenna spectrum
    if (cfg.enable_bandpass) {
        const double fc = (bscan.metadata.antenna_freq_mhz > 10.0)
                              ? bscan.metadata.antenna_freq_mhz
                              : 400.0;
        const double f_low = (cfg.low_cut_mhz > 0.0) ? cfg.low_cut_mhz : (0.25 * fc);
        const double f_high = (cfg.high_cut_mhz > 0.0) ? cfg.high_cut_mhz : (2.50 * fc);
        apply_bandpass(bscan, f_low, f_high, cfg.butterworth_order);
    }

    // 4. Horizontal background / direct-wave clutter subtraction
    if (cfg.enable_background_removal && bscan.num_traces() > 2) {
        remove_background(bscan, cfg.background_window_traces);
    }

    // 5. Depth / time-varying SEC attenuation compensation
    if (cfg.enable_sec_gain) {
        apply_sec_gain(
            bscan,
            cfg.linear_gain_per_ns,
            cfg.exp_alpha_per_ns,
            cfg.max_gain);
    }

    // 6. Optional 2D Kirchhoff migration to collapse pipe/conduit hyperbolas
    if (cfg.enable_migration && bscan.num_traces() > 3) {
        apply_kirchhoff_migration(bscan, cfg.migration_aperture_m);
    }

    // 7. Instantaneous Hilbert envelope for energy/reflector interpretation
    if (cfg.compute_envelope) {
        compute_envelopes(bscan);
    }
}

void SignalProcessor::apply_dewow(BScan& bscan, double window_ns) {
    if (bscan.empty()) {
        return;
    }
    const double dt = bscan.metadata.sample_interval_ns;
    const std::size_t half_win = (window_ns > 0.0 && dt > 0.0)
                                     ? std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(0.5 * window_ns / dt)))
                                     : 0;

    std::vector<double> prefix;
    for (auto& tr : bscan.traces) {
        const std::size_t n = tr.samples.size();
        if (n == 0) {
            continue;
        }
        if (half_win == 0 || half_win >= n) {
            double mean = 0.0;
            for (double v : tr.samples) {
                mean += v;
            }
            mean /= static_cast<double>(n);
            for (double& v : tr.samples) {
                v -= mean;
            }
            continue;
        }

        prefix.assign(n + 1, 0.0);
        for (std::size_t j = 0; j < n; ++j) {
            prefix[j + 1] = prefix[j] + tr.samples[j];
        }
        for (std::size_t j = 0; j < n; ++j) {
            const std::size_t a = (j > half_win) ? (j - half_win) : 0;
            const std::size_t b = std::min(n - 1, j + half_win);
            const double local_mean = (prefix[b + 1] - prefix[a]) / static_cast<double>(b - a + 1);
            tr.samples[j] -= local_mean;
        }
    }
}

void SignalProcessor::apply_time_zero(
    BScan& bscan,
    std::optional<double> fixed_t0_ns,
    double threshold_ratio) {
    if (bscan.empty() || bscan.metadata.sample_interval_ns <= 0.0) {
        return;
    }

    const double dt = bscan.metadata.sample_interval_ns;
    const std::size_t ns = bscan.num_samples();
    if (ns == 0) {
        return;
    }

    std::size_t shift = 0;
    if (fixed_t0_ns.has_value()) {
        if (*fixed_t0_ns > 0.0) {
            shift = static_cast<std::size_t>(std::lround(*fixed_t0_ns / dt));
        }
    } else if (bscan.metadata.time_zero_ns > 0.0) {
        shift = static_cast<std::size_t>(std::lround(bscan.metadata.time_zero_ns / dt));
    } else {
        // Auto-detect first break on stacked mean trace within upper 40% of window
        const std::size_t search_len = std::max<std::size_t>(1, (ns * 2) / 5);
        std::vector<double> mean_tr(search_len, 0.0);
        for (const auto& tr : bscan.traces) {
            const std::size_t lim = std::min(search_len, tr.samples.size());
            for (std::size_t j = 0; j < lim; ++j) {
                mean_tr[j] += tr.samples[j];
            }
        }
        double max_abs = 0.0;
        for (double& v : mean_tr) {
            v /= static_cast<double>(bscan.traces.size());
            max_abs = std::max(max_abs, std::abs(v));
        }

        const double thresh = std::clamp(threshold_ratio, 0.02, 0.8) * max_abs;
        if (thresh > 0.0) {
            for (std::size_t j = 0; j < search_len; ++j) {
                if (std::abs(mean_tr[j]) >= thresh) {
                    shift = j;
                    break;
                }
            }
        }
    }

    if (shift == 0 || shift >= ns) {
        bscan.metadata.time_zero_ns = 0.0;
        return;
    }

    for (auto& tr : bscan.traces) {
        if (tr.samples.size() <= shift) {
            std::fill(tr.samples.begin(), tr.samples.end(), 0.0);
            continue;
        }
        std::move(tr.samples.begin() + static_cast<std::ptrdiff_t>(shift),
                  tr.samples.end(),
                  tr.samples.begin());
        std::fill(tr.samples.end() - static_cast<std::ptrdiff_t>(shift), tr.samples.end(), 0.0);
    }
    bscan.metadata.time_zero_ns = 0.0;
}

void SignalProcessor::apply_bandpass(BScan& bscan, double low_mhz, double high_mhz, int order) {
    if (bscan.empty() || bscan.metadata.sample_interval_ns <= 0.0) {
        return;
    }
    const std::size_t ns = bscan.num_samples();
    if (ns < 4) {
        return;
    }

    const double fs_mhz = 1000.0 / bscan.metadata.sample_interval_ns;
    const double nyquist_mhz = 0.5 * fs_mhz;
    const double f_lo = std::clamp(low_mhz, 0.0, nyquist_mhz * 0.95);
    const double f_hi = std::clamp(high_mhz, f_lo + 1.0, nyquist_mhz * 0.99);
    const int ord = std::clamp(order, 1, 12);

    const std::size_t n_fft = next_power_of_two(ns);
    std::vector<double> gain(n_fft, 0.0);

    for (std::size_t k = 1; k <= n_fft / 2; ++k) {
        const double freq = (static_cast<double>(k) * fs_mhz) / static_cast<double>(n_fft);
        const double hp = (f_lo > 0.0)
                              ? (1.0 / std::sqrt(1.0 + std::pow(f_lo / freq, 2 * ord)))
                              : 1.0;
        const double lp = 1.0 / std::sqrt(1.0 + std::pow(freq / f_hi, 2 * ord));
        const double g = hp * lp;
        gain[k] = g;
        if (k < n_fft - k) {
            gain[n_fft - k] = g;
        }
    }

    std::vector<std::complex<double>> buf(n_fft);
    for (auto& tr : bscan.traces) {
        std::fill(buf.begin(), buf.end(), std::complex<double>{0.0, 0.0});
        const std::size_t m = std::min(ns, tr.samples.size());
        for (std::size_t j = 0; j < m; ++j) {
            buf[j] = {tr.samples[j], 0.0};
        }
        auto spec = fft_forward(buf);
        for (std::size_t k = 0; k < spec.size(); ++k) {
            spec[k] *= gain[k];
        }
        auto inv = fft_inverse(spec);
        for (std::size_t j = 0; j < m; ++j) {
            tr.samples[j] = inv[j].real();
        }
    }
}

void SignalProcessor::remove_background(BScan& bscan, std::size_t sliding_window_traces) {
    const std::size_t n_traces = bscan.num_traces();
    const std::size_t ns = bscan.num_samples();
    if (n_traces < 2 || ns == 0) {
        return;
    }

    if (sliding_window_traces == 0 || sliding_window_traces >= n_traces) {
        std::vector<double> mean_trace(ns, 0.0);
        for (const auto& tr : bscan.traces) {
            const std::size_t m = std::min(ns, tr.samples.size());
            for (std::size_t j = 0; j < m; ++j) {
                mean_trace[j] += tr.samples[j];
            }
        }
        const double inv_n = 1.0 / static_cast<double>(n_traces);
        for (double& v : mean_trace) {
            v *= inv_n;
        }
        for (auto& tr : bscan.traces) {
            const std::size_t m = std::min(ns, tr.samples.size());
            for (std::size_t j = 0; j < m; ++j) {
                tr.samples[j] -= mean_trace[j];
            }
        }
        return;
    }

    // Sliding horizontal window background removal
    const std::size_t half_w = std::max<std::size_t>(1, sliding_window_traces / 2);
    std::vector<std::vector<double>> orig(n_traces);
    for (std::size_t i = 0; i < n_traces; ++i) {
        orig[i] = bscan.traces[i].samples;
    }

    for (std::size_t j = 0; j < ns; ++j) {
        std::vector<double> psum(n_traces + 1, 0.0);
        for (std::size_t i = 0; i < n_traces; ++i) {
            const double val = (j < orig[i].size()) ? orig[i][j] : 0.0;
            psum[i + 1] = psum[i] + val;
        }
        for (std::size_t i = 0; i < n_traces; ++i) {
            if (j >= bscan.traces[i].samples.size()) {
                continue;
            }
            const std::size_t a = (i > half_w) ? (i - half_w) : 0;
            const std::size_t b = std::min(n_traces - 1, i + half_w);
            const double local_bg = (psum[b + 1] - psum[a]) / static_cast<double>(b - a + 1);
            bscan.traces[i].samples[j] -= local_bg;
        }
    }
}

void SignalProcessor::apply_sec_gain(
    BScan& bscan,
    double linear_per_ns,
    double exp_alpha_per_ns,
    double max_gain) {
    if (bscan.empty()) {
        return;
    }
    const double dt = bscan.metadata.sample_interval_ns;
    const std::size_t ns = bscan.num_samples();
    std::vector<double> gain_curve(ns, 1.0);

    for (std::size_t j = 0; j < ns; ++j) {
        const double t_ns = static_cast<double>(j) * dt;
        const double g = (1.0 + linear_per_ns * t_ns) * std::exp(exp_alpha_per_ns * t_ns);
        gain_curve[j] = std::min(g, max_gain);
    }

    for (auto& tr : bscan.traces) {
        const std::size_t m = std::min(ns, tr.samples.size());
        for (std::size_t j = 0; j < m; ++j) {
            tr.samples[j] *= gain_curve[j];
        }
    }
}

void SignalProcessor::apply_kirchhoff_migration(BScan& bscan, double aperture_m) {
    const std::size_t n_traces = bscan.num_traces();
    const std::size_t ns = bscan.num_samples();
    if (n_traces < 3 || ns < 4 || aperture_m <= 0.0) {
        return;
    }

    const double eps_r = std::max(1.0, bscan.metadata.relative_permittivity);
    const double v_m_ns = kSpeedOfLightMPerNs / std::sqrt(eps_r);
    const double dt = bscan.metadata.sample_interval_ns;

    std::vector<std::vector<double>> input(n_traces);
    std::vector<double> track_x(n_traces, 0.0);
    for (std::size_t i = 0; i < n_traces; ++i) {
        input[i] = bscan.traces[i].samples;
        track_x[i] = bscan.traces[i].distance_along_track_m;
    }

    for (std::size_t i0 = 0; i0 < n_traces; ++i0) {
        const double x0 = track_x[i0];
        auto& out_samples = bscan.traces[i0].samples;
        out_samples.assign(ns, 0.0);

        // Determine trace aperture bounds [i_start, i_end]
        std::size_t i_start = i0;
        while (i_start > 0 && (x0 - track_x[i_start - 1]) <= aperture_m) {
            --i_start;
        }
        std::size_t i_end = i0;
        while (i_end + 1 < n_traces && (track_x[i_end + 1] - x0) <= aperture_m) {
            ++i_end;
        }

        for (std::size_t j0 = 1; j0 < ns; ++j0) {
            const double t0 = static_cast<double>(j0) * dt;
            const double z0 = 0.5 * v_m_ns * t0;
            double acc = 0.0;
            double w_sum = 0.0;

            for (std::size_t i = i_start; i <= i_end; ++i) {
                const double dx = track_x[i] - x0;
                const double R = std::hypot(dx, z0);
                const double t_diff = (2.0 * R) / v_m_ns;
                const double s_idx = t_diff / dt;
                const auto k = static_cast<std::size_t>(s_idx);
                if (k + 1 >= input[i].size()) {
                    continue;
                }
                const double frac = s_idx - static_cast<double>(k);
                const double val = input[i][k] * (1.0 - frac) + input[i][k + 1] * frac;

                // Obliquity factor cos(theta) = z0 / R
                const double obliquity = z0 / R;
                acc += obliquity * val;
                w_sum += obliquity;
            }

            if (w_sum > 0.0) {
                out_samples[j0] = acc / std::sqrt(w_sum);
            }
        }
    }
}

void SignalProcessor::compute_envelopes(BScan& bscan) {
    for (auto& tr : bscan.traces) {
        tr.envelope = hilbert_envelope(tr.samples);
    }
}

}  // namespace gpr3d::dsp

