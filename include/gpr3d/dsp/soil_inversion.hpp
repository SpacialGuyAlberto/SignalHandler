#pragma once

#include "gpr3d/core/types.hpp"
#include "gpr3d/geo/material_db.hpp"
#include "gpr3d/scene/objects_3d.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace gpr3d::dsp {

/// Input descriptor for a tracked stratigraphic horizon passed to Fresnel multilayer inversion.
struct HorizonBoundaryInput {
    std::vector<std::size_t> sample_indices{};  ///< Sample index along each trace of the B-scan
    double mean_depth_m{0.0};                   ///< Mean interface depth below surface (m)
};

/// Geophysical subsurface inversion engine: diffraction hyperbola velocity semblance,
/// multilayer Fresnel reflectance inversion, spectral/envelope attenuation estimation,
/// and probabilistic ASTM/GPR soil & material classification.
class SoilInverter {
public:
    /// Exact normal-incidence Fresnel permittivity inversion:
    /// R_k = (sqrt(eps_k) - sqrt(eps_{k+1})) / (sqrt(eps_k) + sqrt(eps_{k+1}))
    /// => eps_{k+1} = eps_k * ((1 - R_k) / (1 + R_k))^2
    [[nodiscard]] static double invert_fresnel_permittivity(
        double eps_r_upper,
        double reflection_coeff_R) noexcept;

    /// Fit local EM propagation velocity v and relative permittivity eps_r around a diffraction
    /// apex (apex_trace_idx, apex_sample_idx) on an unmigrated B-scan via Neidell-Taner Semblance
    /// coherency maximization and sub-grid parabolic refinement.
    [[nodiscard]] static scene::HyperbolaFitResult fit_hyperbola_velocity(
        const BScan& unmigrated_bscan,
        std::size_t apex_trace_idx,
        std::size_t apex_sample_idx,
        double half_aperture_m = 1.0,
        double target_radius_m = 0.0);

    /// Automatically locate candidate diffraction apexes on an unmigrated background-removed B-scan
    /// and fit local subsurface velocities across the profile.
    [[nodiscard]] static std::vector<scene::HyperbolaFitResult> scan_and_fit_hyperbolas(
        const BScan& unmigrated_bscan,
        double min_depth_m = 0.12,
        double max_depth_m = 5.0);

    /// Estimate frequency-dependent attenuation alpha (dB/m), loss tangent tan(delta), and
    /// apparent conductivity sigma (mS/m) combining geometric-compensated amplitude decay and
    /// Quan-Harris spectral centroid frequency downshift.
    [[nodiscard]] static scene::AttenuationEstimate estimate_trace_attenuation(
        const BScan& conditioned_no_gain_bscan,
        double eps_r,
        double min_depth_m = 0.06,
        double max_depth_m = 5.0);

    /// Invert per-layer dielectric permittivities (eps_{r,1}, eps_{r,2}, ...), wave velocities,
    /// conductivities, and ASTM/GPR material classifications across stratigraphic horizons.
    [[nodiscard]] static std::vector<scene::LithologicalLayer> invert_stratigraphic_layers(
        const BScan& conditioned_no_gain_bscan,
        std::span<const HorizonBoundaryInput> horizon_boundaries,
        double surface_epsr,
        const scene::AttenuationEstimate& bulk_atten,
        double max_profile_depth_m);

    /// Perform complete standalone subsurface dielectric & lithological inversion on a raw B-scan
    /// (ideal for `gpr3d inspect` and rapid geotechnical diagnostics).
    [[nodiscard]] static scene::SubsurfaceMaterialReport analyze_subsurface(
        const BScan& raw_bscan,
        double min_depth_m = 0.10,
        double max_depth_m = 5.0);
};

}  // namespace gpr3d::dsp

