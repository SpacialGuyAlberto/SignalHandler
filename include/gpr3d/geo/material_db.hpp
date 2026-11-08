#pragma once

#include "gpr3d/core/types.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace gpr3d::geo {

/// Standard geophysical & civil-engineering subsurface material categories (ASTM D6432 / GPR reference).
enum class MaterialKind {
    Air_Void,                    ///< Subsurface void / empty cavity (eps_r ~ 1.0)
    Asphalt_Dry,                 ///< Dry asphalt / bituminous pavement (eps_r 3.0 - 5.0, sigma < 2 mS/m)
    Concrete_Cured,              ///< Dry / cured Portland cement concrete (eps_r 5.0 - 7.5, sigma 2 - 10 mS/m)
    Concrete_Moist,              ///< Moist / reinforced concrete slab (eps_r 8.0 - 12.0, sigma 10 - 50 mS/m)
    Gravel_Dry,                  ///< Compacted dry gravel / roadbase subbase (eps_r 4.0 - 6.0, low loss)
    Sand_Dry,                    ///< Dry sand / sandy fill (eps_r 3.5 - 6.0, very low loss)
    Bedrock_Limestone_Granite,   ///< Competent bedrock: limestone / granite (eps_r 5.0 - 8.5, low loss)
    Silt_Loam,                   ///< Silty / loamy soil (eps_r 7.0 - 12.0, moderate loss)
    Sand_Saturated,              ///< Moist / water-saturated sand (eps_r 12.0 - 22.0, moderate loss)
    Clay_Wet,                    ///< Wet / plastic clay (eps_r 15.0 - 35.0, high attenuation sigma > 50 mS/m)
    Water_Fresh,                 ///< Fresh water / water table lens (eps_r ~ 80)
    Conduit_Polymer_PVC_PE,      ///< Polymer utility duct PVC / HDPE / fiber-optic microduct (eps_r 2.2 - 3.2)
    Conduit_Metallic,            ///< Metallic pipe / conductive cable (PEC, R ~ -1.0)
    Unknown
};

/// Reference electromagnetic & visual specification for a cataloged subsurface material.
struct MaterialSpec {
    MaterialKind kind{MaterialKind::Unknown};
    const char* code{"unknown"};
    const char* name_es{"Material indeterminado"};
    const char* name_en{"Unknown Material"};

    double epsr_min{1.0};
    double epsr_max{81.0};
    double sigma_min_ms_m{0.01};
    double sigma_max_ms_m{100.0};

    bool is_stratigraphic_medium{true};  ///< True if valid as a continuous bulk soil/pavement layer
    std::array<float, 4> color_rgba{0.65f, 0.65f, 0.65f, 0.70f};
    float pbr_metallic{0.05f};
    float pbr_roughness{0.82f};
};

/// Probabilistic classification result matching observed EM properties against the reference catalog.
struct MaterialMatch {
    MaterialKind kind{MaterialKind::Unknown};
    std::string code{"unknown"};
    std::string name_es{"Material indeterminado"};
    std::string name_en{"Unknown Material"};
    double probability{0.0};                   ///< Normalized posterior confidence in [0.0, 1.0]
    double estimated_epsr{9.0};                ///< Estimated relative permittivity eps_r
    double estimated_velocity_m_ns{0.10};      ///< Estimated wave velocity v (m/ns)
    double estimated_attenuation_db_m{0.0};    ///< Estimated one-way attenuation alpha (dB/m)
    double estimated_conductivity_ms_m{0.0};   ///< Estimated apparent conductivity sigma (mS/m)
    double loss_tangent{0.0};                  ///< Estimated loss tangent tan(delta)
    std::array<float, 4> color_rgba{0.65f, 0.65f, 0.65f, 0.70f};
    float pbr_metallic{0.05f};
    float pbr_roughness{0.82f};

    /// Format RGB color as "#RRGGBB" CSS/GIS hex string.
    [[nodiscard]] std::string color_hex() const;
};

/// Geophysical database and Bayesian classifier for subsurface soils, pavements, and buried utilities.
class MaterialDatabase {
public:
    /// Access the full ASTM / GPR reference catalog of materials.
    [[nodiscard]] static std::span<const MaterialSpec> catalog() noexcept;

    /// Lookup reference specification for a specific `MaterialKind`.
    [[nodiscard]] static const MaterialSpec& spec_for(MaterialKind kind) noexcept;

    /// Rank continuous pavement/soil layer hypotheses given estimated permittivity, conductivity, and depth.
    [[nodiscard]] static std::vector<MaterialMatch> rank_soil_materials(
        double eps_r,
        double conductivity_ms_m,
        double attenuation_db_m,
        double loss_tangent = 0.0,
        double depth_top_m = 0.0,
        std::size_t max_results = 3);

    /// Return the single highest-probability pavement/soil classification for a stratigraphic layer.
    [[nodiscard]] static MaterialMatch classify_soil_layer(
        double eps_r,
        double conductivity_ms_m,
        double attenuation_db_m,
        double loss_tangent = 0.0,
        double depth_top_m = 0.0);

    /// Classify a cylindrical buried conduit / utility from reflection phase polarity, contrast, and host medium.
    [[nodiscard]] static MaterialMatch classify_conduit(
        bool negative_metallic_polarity,
        double normalized_intensity,
        double host_epsr,
        double host_attenuation_db_m = 2.0);

    /// Classify a localized 3D subsurface anomaly (void, boulder, metallic lid, moisture pocket).
    [[nodiscard]] static MaterialMatch classify_anomaly(
        bool negative_metallic_polarity,
        double normalized_intensity,
        double host_epsr,
        double depth_m,
        double host_attenuation_db_m = 2.0);
};

}  // namespace gpr3d::geo

