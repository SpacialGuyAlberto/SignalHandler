#pragma once

#include "gpr3d/core/types.hpp"

#include <cstddef>
#include <optional>

namespace gpr3d::dsp {

/// Configurable parameters for the GPR signal conditioning & migration pipeline.
struct ProcessingConfig {
    bool enable_time_zero{true};
    std::optional<double> fixed_time_zero_ns;  ///< If nullopt, auto-detects first-break arrival
    double first_break_threshold{0.15};        ///< Fraction of peak amplitude for auto first-break [0..1]

    bool enable_dewow{true};
    double dewow_window_ns{4.0};               ///< Running-mean window (ns) for inductive wow removal

    bool enable_bandpass{true};
    double low_cut_mhz{0.0};                   ///< 0 = auto (0.25 * antenna_freq_mhz)
    double high_cut_mhz{0.0};                  ///< 0 = auto (2.50 * antenna_freq_mhz)
    int butterworth_order{4};

    bool enable_background_removal{true};
    std::size_t background_window_traces{0};   ///< 0 = global mean trace; >0 = sliding trace window

    bool enable_sec_gain{true};
    double linear_gain_per_ns{0.08};           ///< Spherical spreading compensation coefficient
    double exp_alpha_per_ns{0.04};             ///< Dielectric attenuation exponent (Np/ns)
    double max_gain{60.0};                     ///< Gain ceiling to prevent deep noise blow-up

    bool enable_migration{false};              ///< Optional 2D Kirchhoff diffraction migration
    double migration_aperture_m{1.5};          ///< Horizontal half-aperture (m) for Kirchhoff summation

    bool compute_envelope{true};               ///< Compute instantaneous Hilbert amplitude envelope
};

/// Modular GPR Digital Signal Processor for B-scan conditioning and migration.
class SignalProcessor {
public:
    /// Run the configured processing pipeline in-place on a B-scan.
    static void process(BScan& bscan, const ProcessingConfig& cfg = {});

    /// Shift traces vertically so ground surface arrival aligns with t = 0 ns.
    static void apply_time_zero(
        BScan& bscan,
        std::optional<double> fixed_t0_ns = std::nullopt,
        double threshold_ratio = 0.15);

    /// Remove DC bias and low-frequency inductive "wow" distortion per trace.
    static void apply_dewow(BScan& bscan, double window_ns);

    /// Apply zero-phase frequency-domain Butterworth bandpass filter [low_mhz, high_mhz].
    static void apply_bandpass(BScan& bscan, double low_mhz, double high_mhz, int order = 4);

    /// Subtract mean horizontal clutter / direct air-ground coupling wave.
    static void remove_background(BScan& bscan, std::size_t sliding_window_traces = 0);

    /// Apply Spreading and Exponential Compensation (SEC) depth-dependent gain.
    static void apply_sec_gain(
        BScan& bscan,
        double linear_per_ns,
        double exp_alpha_per_ns,
        double max_gain = 60.0);

    /// Collapse diffraction hyperbolas via 2D Kirchhoff time migration using medium velocity.
    static void apply_kirchhoff_migration(BScan& bscan, double aperture_m = 1.5);

    /// Populate `AScan::envelope` across all traces using FFT analytic signal.
    static void compute_envelopes(BScan& bscan);
};

}  // namespace gpr3d::dsp

