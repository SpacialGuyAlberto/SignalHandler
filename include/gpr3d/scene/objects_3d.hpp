#pragma once

#include "gpr3d/core/types.hpp"
#include "gpr3d/geo/material_db.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gpr3d::scene {

/// 3D Cartesian vector (used for local ENU vertex coordinates and unit normals).
struct Vec3 {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] Vec3 operator+(const Vec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    [[nodiscard]] Vec3 operator-(const Vec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    [[nodiscard]] Vec3 operator*(double s) const noexcept { return {x * s, y * s, z * s}; }

    [[nodiscard]] double norm() const noexcept { return std::sqrt(x * x + y * y + z * z); }

    [[nodiscard]] Vec3 normalized() const noexcept {
        const double n = norm();
        return (n > 1e-12) ? Vec3{x / n, y / n, z / n} : Vec3{0.0, 0.0, 1.0};
    }

    [[nodiscard]] static double dot(const Vec3& a, const Vec3& b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    [[nodiscard]] static Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }
};

/// Linear RGBA material color in [0, 1].
struct ColorRgba {
    float r{0.8f};
    float g{0.8f};
    float b{0.8f};
    float a{1.0f};

    [[nodiscard]] static constexpr ColorRgba from_array(
        const std::array<float, 4>& rgba,
        float alpha_override = -1.0f) noexcept {
        return {
            rgba[0],
            rgba[1],
            rgba[2],
            (alpha_override >= 0.0f) ? alpha_override : rgba[3]
        };
    }
};

/// Semantic category of a reconstructed 3D subsurface object.
enum class SemanticKind {
    Conduit,             ///< Cylindrical buried utility, cable, or fiber-optic duct
    StratigraphicLayer,  ///< Continuous subsurface pavement or soil horizon interface
    SubsurfaceAnomaly,   ///< Localized 3D reflector volume (cavity, boulder, junction box)
    GroundSurface        ///< Surveyed terrain / street reference surface
};

/// Electromagnetic reflection phase polarity at a target interface.
enum class PolarityKind {
    AirFilledDuctOrVoid,          ///< Positive reflection coefficient (eps_target < eps_soil, e.g. empty HDPE duct)
    MetallicOrHighPermittivity,   ///< Negative phase inversion (PEC metal pipe/cable or water-filled pipe)
    Mixed                         ///< Transitional or composite phase signature
};

[[nodiscard]] inline const char* to_string(PolarityKind p) noexcept {
    switch (p) {
        case PolarityKind::AirFilledDuctOrVoid:
            return "air_duct_or_void";
        case PolarityKind::MetallicOrHighPermittivity:
            return "metallic_or_water";
        case PolarityKind::Mixed:
            return "mixed";
    }
    return "mixed";
}

/// Indexed 3D triangle mesh expressed in Local ENU metric coordinates (x=East, y=North, z=Up).
struct Mesh3D {
    std::string name{};
    SemanticKind kind{SemanticKind::SubsurfaceAnomaly};
    ColorRgba color{};
    float metallic{0.1f};
    float roughness{0.6f};
    std::vector<Vec3> vertices{};
    std::vector<Vec3> normals{};
    std::vector<std::array<std::uint32_t, 3>> triangles{};

    [[nodiscard]] bool empty() const noexcept { return vertices.empty() || triangles.empty(); }
};

/// Reconstructed 3D cylindrical pipe / fiber-optic conduit.
struct ConduitObject3D {
    std::size_t id{0};
    std::string label{};
    PolarityKind polarity{PolarityKind::Mixed};
    double estimated_radius_m{0.05};
    double mean_depth_m{0.0};
    double peak_intensity{0.0};
    double confidence{0.0};
    geo::MaterialMatch material{};

    std::vector<Wgs84Coord> centerline_wgs84{};
    std::vector<UtmCoord> centerline_utm{};
    std::vector<EnuCoord> centerline_enu{};
    Mesh3D mesh{};
};

/// Reconstructed 3D stratigraphic horizon / soil-pavement interface surface.
struct LayerHorizonObject3D {
    std::size_t id{0};
    std::string label{};
    double mean_depth_m{0.0};
    double dip_slope_m_per_m{0.0};
    double mean_intensity{0.0};
    double fresnel_reflection_coeff{0.0};
    geo::MaterialMatch material{};  ///< Classified lithology of the stratum starting at this horizon

    std::vector<Wgs84Coord> profile_wgs84{};
    std::vector<EnuCoord> profile_enu{};
    Mesh3D mesh{};
};

/// Reconstructed localized 3D subsurface anomaly (ellipsoidal volume / cavity / block).
struct AnomalyObject3D {
    std::size_t id{0};
    std::string label{};
    PolarityKind polarity{PolarityKind::Mixed};
    Wgs84Coord center_wgs84{};
    UtmCoord center_utm{};
    EnuCoord center_enu{};
    Vec3 radii_m{0.15, 0.15, 0.10};
    double depth_m{0.0};
    double peak_intensity{0.0};
    geo::MaterialMatch material{};
    Mesh3D mesh{};
};

/// Fitted diffraction hyperbola velocity & dielectric estimate around a localized apex.
struct HyperbolaFitResult {
    bool valid{false};
    std::size_t trace_idx{0};
    std::size_t sample_idx{0};
    double x0_m{0.0};
    double t0_ns{0.0};
    double depth_m{0.0};
    double velocity_m_ns{0.10};     ///< Fitted EM propagation velocity v (m/ns)
    double estimated_epsr{9.0};     ///< Relative permittivity eps_r = (c0 / v)^2
    double semblance{0.0};          ///< Neidell-Taner coherency S(v*) in [0, 1]
    double confidence{0.0};         ///< Prominence-weighted fit quality in [0, 1]
};

/// Frequency-dependent electromagnetic attenuation & loss parameters of a subsurface zone.
struct AttenuationEstimate {
    double attenuation_db_per_m{2.0};     ///< One-way attenuation alpha (dB/m)
    double attenuation_np_per_m{0.23};    ///< One-way attenuation alpha (Np/m)
    double loss_tangent{0.02};            ///< Dielectric loss tangent tan(delta)
    double conductivity_ms_m{3.0};        ///< Apparent electrical conductivity sigma (mS/m)
    double effective_freq_mhz{400.0};     ///< Measured effective spectral center frequency (MHz)
    double centroid_shift_mhz_per_m{0.0}; ///< Quan-Harris spectral centroid downshift df_c/dz (MHz/m)
    bool hardware_gain_detected{false};   ///< True if raw traces already had field range-gain applied
};

/// Inverted subsurface lithological layer in the vertical stratigraphic column.
struct LithologicalLayer {
    std::size_t layer_index{1};           ///< 1-based layer index from surface downward
    double depth_top_m{0.0};              ///< Upper boundary depth (m)
    double depth_bottom_m{1.0};           ///< Lower boundary depth (m)
    double thickness_m{1.0};              ///< Layer thickness (m)
    double reflection_coeff_top{0.0};     ///< Normal-incidence Fresnel coefficient R at top interface
    double estimated_epsr{9.0};           ///< Inverted relative permittivity eps_r
    double velocity_m_ns{0.10};           ///< Wave velocity in this layer (m/ns)
    double attenuation_db_m{2.0};         ///< Estimated one-way attenuation in layer (dB/m)
    double conductivity_ms_m{3.0};        ///< Apparent electrical conductivity sigma (mS/m)
    double loss_tangent{0.02};            ///< Dielectric loss tangent tan(delta)
    geo::MaterialMatch primary_material{};
    std::vector<geo::MaterialMatch> candidates{};
};

/// Complete subsurface electromagnetic inversion & soil/material classification report.
struct SubsurfaceMaterialReport {
    double nominal_header_epsr{9.0};
    double inverted_bulk_epsr{9.0};
    double inverted_bulk_velocity_m_ns{0.10};
    double mean_hyperbola_semblance{0.0};
    std::vector<HyperbolaFitResult> hyperbola_fits{};
    AttenuationEstimate bulk_attenuation{};
    std::vector<LithologicalLayer> strata{};
    geo::MaterialMatch dominant_soil{};
};

/// Detailed waveform & spectral fluctuation metrics for a single A-scan trace.
struct TraceFluctuationMetrics {
    std::size_t trace_id{0};
    double distance_m{0.0};
    double rms_amplitude{0.0};
    double peak_to_peak{0.0};
    double crest_factor{0.0};
    double dominant_freq_mhz{0.0};
    double spectral_centroid_mhz{0.0};
    double peak_envelope_depth_m{0.0};
    double energy_decay_db{0.0};
};

/// Graphical & analytical summary of signal fluctuations across a processed GPR profile.
struct SignalFluctuationSummary {
    std::size_t representative_trace_idx{0};     ///< Index of trace with strongest subsurface target
    std::vector<double> time_axis_ns{};
    std::vector<double> depth_axis_m{};
    std::vector<double> rep_waveform{};          ///< Representative A-scan signed amplitude s(t)
    std::vector<double> rep_envelope{};          ///< Instantaneous Hilbert envelope |z(t)|
    std::vector<double> rep_inst_phase_rad{};    ///< Instantaneous phase arg(z(t)) in [-pi, pi]
    std::vector<double> rep_inst_freq_mhz{};     ///< Smoothed instantaneous frequency (MHz)

    std::vector<double> freq_axis_mhz{};         ///< Positive FFT frequency axis (MHz)
    std::vector<double> mean_psd_db{};           ///< Ensemble mean power spectral density (dB, normalized to 0 dB peak)

    std::vector<TraceFluctuationMetrics> per_trace{};
};

/// Complete interpreted 3D subsurface scene combining semantic 3D meshes, georeferencing,
/// electromagnetic material inversion, underlying 3D point cloud, and signal fluctuation analytics.
struct Scene3D {
    Wgs84Coord origin_wgs84{};
    UtmCoord origin_utm{};
    bool has_valid_crs{false};
    std::string description{};

    Mesh3D ground_surface{};
    std::vector<ConduitObject3D> conduits{};
    std::vector<LayerHorizonObject3D> horizons{};
    std::vector<AnomalyObject3D> anomalies{};
    PointCloud3D point_cloud{};
    BScan primary_processed_bscan{};
    SignalFluctuationSummary fluctuations{};
    SubsurfaceMaterialReport subsurface{};

    [[nodiscard]] std::size_t total_objects() const noexcept {
        return conduits.size() + horizons.size() + anomalies.size();
    }
};

}  // namespace gpr3d::scene
