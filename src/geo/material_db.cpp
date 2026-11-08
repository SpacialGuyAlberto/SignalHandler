#include "gpr3d/geo/material_db.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace gpr3d::geo {

namespace {

constexpr std::array<MaterialSpec, 14> kMaterialCatalog{{
    {
        .kind = MaterialKind::Air_Void,
        .code = "air_void",
        .name_es = "Cavidad / Hueco con aire",
        .name_en = "Air Void / Subsurface Cavity",
        .epsr_min = 1.0,
        .epsr_max = 1.6,
        .sigma_min_ms_m = 0.001,
        .sigma_max_ms_m = 0.05,
        .is_stratigraphic_medium = false,
        .color_rgba = {0.30f, 0.88f, 0.96f, 0.62f},
        .pbr_metallic = 0.02f,
        .pbr_roughness = 0.35f
    },
    {
        .kind = MaterialKind::Asphalt_Dry,
        .code = "asphalt_dry",
        .name_es = "Asfalto seco / Capa de rodadura",
        .name_en = "Dry Asphalt Pavement",
        .epsr_min = 3.0,
        .epsr_max = 5.0,
        .sigma_min_ms_m = 0.05,
        .sigma_max_ms_m = 2.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.22f, 0.24f, 0.28f, 0.72f},
        .pbr_metallic = 0.05f,
        .pbr_roughness = 0.88f
    },
    {
        .kind = MaterialKind::Concrete_Cured,
        .code = "concrete_cured",
        .name_es = "Hormigón seco / Losa curada",
        .name_en = "Cured Dry Concrete",
        .epsr_min = 5.0,
        .epsr_max = 7.5,
        .sigma_min_ms_m = 1.5,
        .sigma_max_ms_m = 10.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.62f, 0.65f, 0.68f, 0.68f},
        .pbr_metallic = 0.06f,
        .pbr_roughness = 0.80f
    },
    {
        .kind = MaterialKind::Concrete_Moist,
        .code = "concrete_moist",
        .name_es = "Hormigón húmedo / Losa estructural",
        .name_en = "Moist / Reinforced Concrete",
        .epsr_min = 8.0,
        .epsr_max = 12.0,
        .sigma_min_ms_m = 10.0,
        .sigma_max_ms_m = 50.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.44f, 0.49f, 0.55f, 0.70f},
        .pbr_metallic = 0.08f,
        .pbr_roughness = 0.72f
    },
    {
        .kind = MaterialKind::Gravel_Dry,
        .code = "gravel_dry",
        .name_es = "Grava compactada / Base granular",
        .name_en = "Compacted Dry Gravel / Roadbase",
        .epsr_min = 4.0,
        .epsr_max = 6.2,
        .sigma_min_ms_m = 0.2,
        .sigma_max_ms_m = 4.5,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.78f, 0.62f, 0.34f, 0.66f},
        .pbr_metallic = 0.04f,
        .pbr_roughness = 0.90f
    },
    {
        .kind = MaterialKind::Sand_Dry,
        .code = "sand_dry",
        .name_es = "Arena seca / Relleno arenoso",
        .name_en = "Dry Sand / Sandy Fill",
        .epsr_min = 3.5,
        .epsr_max = 6.0,
        .sigma_min_ms_m = 0.05,
        .sigma_max_ms_m = 2.5,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.88f, 0.76f, 0.44f, 0.66f},
        .pbr_metallic = 0.02f,
        .pbr_roughness = 0.92f
    },
    {
        .kind = MaterialKind::Bedrock_Limestone_Granite,
        .code = "bedrock_rock",
        .name_es = "Roca consolidada (Caliza / Granito)",
        .name_en = "Consolidated Bedrock (Limestone / Granite)",
        .epsr_min = 5.0,
        .epsr_max = 8.5,
        .sigma_min_ms_m = 0.2,
        .sigma_max_ms_m = 4.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.58f, 0.55f, 0.50f, 0.70f},
        .pbr_metallic = 0.06f,
        .pbr_roughness = 0.85f
    },
    {
        .kind = MaterialKind::Silt_Loam,
        .code = "silt_loam",
        .name_es = "Limo / Suelo franco compactado",
        .name_en = "Silty / Loamy Subgrade Soil",
        .epsr_min = 7.0,
        .epsr_max = 12.5,
        .sigma_min_ms_m = 3.5,
        .sigma_max_ms_m = 32.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.60f, 0.43f, 0.26f, 0.68f},
        .pbr_metallic = 0.03f,
        .pbr_roughness = 0.88f
    },
    {
        .kind = MaterialKind::Sand_Saturated,
        .code = "sand_saturated",
        .name_es = "Arena húmeda / Suelo saturado",
        .name_en = "Water-Saturated Sand / Moist Soil",
        .epsr_min = 12.0,
        .epsr_max = 22.0,
        .sigma_min_ms_m = 5.0,
        .sigma_max_ms_m = 45.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.46f, 0.55f, 0.42f, 0.70f},
        .pbr_metallic = 0.05f,
        .pbr_roughness = 0.74f
    },
    {
        .kind = MaterialKind::Clay_Wet,
        .code = "clay_wet",
        .name_es = "Arcilla húmeda / Suelo cohesivo",
        .name_en = "Wet Plastic Clay",
        .epsr_min = 15.0,
        .epsr_max = 35.0,
        .sigma_min_ms_m = 35.0,
        .sigma_max_ms_m = 300.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.48f, 0.26f, 0.18f, 0.74f},
        .pbr_metallic = 0.05f,
        .pbr_roughness = 0.76f
    },
    {
        .kind = MaterialKind::Water_Fresh,
        .code = "water_fresh",
        .name_es = "Agua dulce / Nivel freático",
        .name_en = "Fresh Water / Water Table",
        .epsr_min = 74.0,
        .epsr_max = 84.0,
        .sigma_min_ms_m = 5.0,
        .sigma_max_ms_m = 80.0,
        .is_stratigraphic_medium = true,
        .color_rgba = {0.14f, 0.52f, 0.92f, 0.70f},
        .pbr_metallic = 0.10f,
        .pbr_roughness = 0.20f
    },
    {
        .kind = MaterialKind::Conduit_Polymer_PVC_PE,
        .code = "conduit_pvc_hdpe",
        .name_es = "Ducto polimérico PVC/PEAD (Fibra óptica)",
        .name_en = "PVC/HDPE Polymer Conduit (Fiber Optic)",
        .epsr_min = 2.2,
        .epsr_max = 3.2,
        .sigma_min_ms_m = 0.001,
        .sigma_max_ms_m = 0.2,
        .is_stratigraphic_medium = false,
        .color_rgba = {0.10f, 0.84f, 0.56f, 1.00f},
        .pbr_metallic = 0.15f,
        .pbr_roughness = 0.35f
    },
    {
        .kind = MaterialKind::Conduit_Metallic,
        .code = "conduit_metallic",
        .name_es = "Tubería metálica / Cable conductor",
        .name_en = "Metallic Pipe / Conductive Utility",
        .epsr_min = 100.0,
        .epsr_max = 10000.0,
        .sigma_min_ms_m = 1000.0,
        .sigma_max_ms_m = 1000000.0,
        .is_stratigraphic_medium = false,
        .color_rgba = {0.92f, 0.24f, 0.20f, 1.00f},
        .pbr_metallic = 0.80f,
        .pbr_roughness = 0.30f
    },
    {
        .kind = MaterialKind::Unknown,
        .code = "unknown",
        .name_es = "Material indeterminado",
        .name_en = "Unknown Material",
        .epsr_min = 1.0,
        .epsr_max = 81.0,
        .sigma_min_ms_m = 0.01,
        .sigma_max_ms_m = 100.0,
        .is_stratigraphic_medium = false,
        .color_rgba = {0.65f, 0.65f, 0.65f, 0.70f},
        .pbr_metallic = 0.05f,
        .pbr_roughness = 0.80f
    }
}};

/// Smooth plateau-Gaussian score: ~1.0 inside [lo, hi] with gentle center preference,
/// decaying smoothly outside the interval with scale `tail_width`.
double interval_likelihood(double val, double lo, double hi, double tail_width) noexcept {
    const double mid = 0.5 * (lo + hi);
    const double half = std::max(1e-6, 0.5 * (hi - lo));
    if (val >= lo && val <= hi) {
        const double u = (val - mid) / half;
        return 0.88 + 0.12 * (1.0 - u * u);
    }
    const double dist = (val < lo) ? (lo - val) : (val - hi);
    const double z = dist / std::max(1e-6, tail_width);
    return 0.88 * std::exp(-0.5 * z * z);
}

/// Geotechnical & civil-engineering depth prior P(Material | depth_top_m).
double stratigraphic_depth_prior(MaterialKind kind, double depth_top_m) noexcept {
    switch (kind) {
        case MaterialKind::Asphalt_Dry:
            if (depth_top_m < 0.06) return 1.35;
            if (depth_top_m < 0.20) return 0.75;
            return 0.12;  // Wearing course asphalt rarely sits below 20 cm
        case MaterialKind::Concrete_Cured:
            if (depth_top_m < 0.08) return 1.30;
            if (depth_top_m < 0.35) return 0.85;
            return 0.30;
        case MaterialKind::Concrete_Moist:
            if (depth_top_m < 0.12) return 1.15;
            if (depth_top_m < 0.50) return 0.85;
            return 0.40;
        case MaterialKind::Gravel_Dry:
            if (depth_top_m >= 0.05 && depth_top_m <= 0.65) return 1.25;  // Roadbase sub-layer
            return 0.90;
        case MaterialKind::Sand_Dry:
            if (depth_top_m >= 0.10) return 1.05;
            return 0.85;
        case MaterialKind::Silt_Loam:
            if (depth_top_m >= 0.20) return 1.18;
            return 0.92;
        case MaterialKind::Bedrock_Limestone_Granite:
            if (depth_top_m >= 0.50) return 1.15;
            return 0.65;
        case MaterialKind::Sand_Saturated:
            if (depth_top_m >= 0.30) return 1.10;
            return 0.80;
        case MaterialKind::Clay_Wet:
            if (depth_top_m >= 0.25) return 1.18;
            return 0.88;
        case MaterialKind::Water_Fresh:
            if (depth_top_m >= 0.40) return 1.00;
            return 0.50;
        default:
            return 0.50;
    }
}

MaterialMatch make_match_from_spec(
    const MaterialSpec& spec,
    double prob,
    double eps_r,
    double sigma_ms_m,
    double atten_db_m,
    double loss_tan) {
    const double clamped_eps = std::max(1.0, eps_r);
    return MaterialMatch{
        .kind = spec.kind,
        .code = spec.code,
        .name_es = spec.name_es,
        .name_en = spec.name_en,
        .probability = std::clamp(prob, 0.0, 1.0),
        .estimated_epsr = clamped_eps,
        .estimated_velocity_m_ns = kSpeedOfLightMPerNs / std::sqrt(clamped_eps),
        .estimated_attenuation_db_m = std::max(0.0, atten_db_m),
        .estimated_conductivity_ms_m = std::max(0.0, sigma_ms_m),
        .loss_tangent = std::max(0.0, loss_tan),
        .color_rgba = spec.color_rgba,
        .pbr_metallic = spec.pbr_metallic,
        .pbr_roughness = spec.pbr_roughness
    };
}

}  // namespace

std::string MaterialMatch::color_hex() const {
    const int r = static_cast<int>(std::clamp(std::lround(color_rgba[0] * 255.0f), 0L, 255L));
    const int g = static_cast<int>(std::clamp(std::lround(color_rgba[1] * 255.0f), 0L, 255L));
    const int b = static_cast<int>(std::clamp(std::lround(color_rgba[2] * 255.0f), 0L, 255L));
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return std::string(buf);
}

std::span<const MaterialSpec> MaterialDatabase::catalog() noexcept {
    return kMaterialCatalog;
}

const MaterialSpec& MaterialDatabase::spec_for(MaterialKind kind) noexcept {
    for (const auto& s : kMaterialCatalog) {
        if (s.kind == kind) {
            return s;
        }
    }
    return kMaterialCatalog.back();
}

std::vector<MaterialMatch> MaterialDatabase::rank_soil_materials(
    double eps_r,
    double conductivity_ms_m,
    double attenuation_db_m,
    double loss_tangent,
    double depth_top_m,
    std::size_t max_results) {
    const double clean_eps = std::clamp(eps_r, 1.0, 85.0);
    const double clean_sigma = std::clamp(conductivity_ms_m, 0.02, 500.0);
    const double log_sigma = std::log10(clean_sigma);

    struct ScoredItem {
        const MaterialSpec* spec{nullptr};
        double raw_score{0.0};
        double fit_quality{0.0};
    };

    std::vector<ScoredItem> scored;
    scored.reserve(kMaterialCatalog.size());
    double sum_scores = 0.0;

    for (const auto& spec : kMaterialCatalog) {
        if (!spec.is_stratigraphic_medium) {
            continue;
        }

        const double eps_span = std::max(1.2, 0.45 * (spec.epsr_max - spec.epsr_min));
        const double l_eps = interval_likelihood(clean_eps, spec.epsr_min, spec.epsr_max, eps_span);

        const double log_s_min = std::log10(std::max(1e-3, spec.sigma_min_ms_m));
        const double log_s_max = std::log10(std::max(1e-2, spec.sigma_max_ms_m));
        const double l_sig = interval_likelihood(log_sigma, log_s_min, log_s_max, 0.55);

        const double prior = stratigraphic_depth_prior(spec.kind, depth_top_m);
        // Permittivity carries primary weight (dielectric velocity), conductivity disambiguates dry vs wet/clay
        const double fit_q = std::pow(l_eps, 1.35) * std::pow(l_sig, 0.65);
        const double post = fit_q * prior;

        scored.push_back({&spec, post, fit_q});
        sum_scores += post;
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredItem& a, const ScoredItem& b) { return a.raw_score > b.raw_score; });

    std::vector<MaterialMatch> results;
    const std::size_t count = std::min(max_results, scored.size());
    results.reserve(count);

    // Temper posterior slightly with best physical fit quality so extreme outliers don't claim 99% confidence
    const double best_fit_cap = scored.empty() ? 0.5 : std::clamp(0.35 + 0.65 * scored.front().fit_quality, 0.20, 0.98);

    for (std::size_t i = 0; i < count; ++i) {
        const double norm_p = (sum_scores > 1e-12) ? (scored[i].raw_score / sum_scores) : 0.0;
        const double calibrated_p = std::clamp(norm_p * best_fit_cap + (i == 0 ? 0.12 * scored[i].fit_quality : 0.0),
                                               0.01, 0.98);
        results.push_back(make_match_from_spec(
            *scored[i].spec,
            calibrated_p,
            clean_eps,
            conductivity_ms_m,
            attenuation_db_m,
            loss_tangent));
    }

    return results;
}

MaterialMatch MaterialDatabase::classify_soil_layer(
    double eps_r,
    double conductivity_ms_m,
    double attenuation_db_m,
    double loss_tangent,
    double depth_top_m) {
    auto ranked = rank_soil_materials(eps_r, conductivity_ms_m, attenuation_db_m, loss_tangent, depth_top_m, 1);
    if (!ranked.empty()) {
        return ranked.front();
    }
    return make_match_from_spec(
        spec_for(MaterialKind::Unknown), 0.0, eps_r, conductivity_ms_m, attenuation_db_m, loss_tangent);
}

MaterialMatch MaterialDatabase::classify_conduit(
    bool negative_metallic_polarity,
    double normalized_intensity,
    double host_epsr,
    double host_attenuation_db_m) {
    const double intensity = std::clamp(normalized_intensity, 0.0, 1.0);
    if (negative_metallic_polarity) {
        const auto& spec = spec_for(MaterialKind::Conduit_Metallic);
        const double conf = std::clamp(0.62 + 0.34 * intensity, 0.50, 0.97);
        auto m = make_match_from_spec(spec, conf, 1000.0, 10000.0, host_attenuation_db_m, 1.0);
        m.estimated_velocity_m_ns = 0.0;  // PEC total reflection
        return m;
    }

    // Positive phase polarity -> lower permittivity dielectric conduit (empty HDPE/PVC fiber duct)
    const auto& spec = spec_for(MaterialKind::Conduit_Polymer_PVC_PE);
    const double contrast_boost = (host_epsr > 4.5) ? 0.08 : 0.02;
    const double conf = std::clamp(0.58 + 0.30 * intensity + contrast_boost, 0.48, 0.95);
    constexpr double est_duct_eps = 2.4;  // Effective HDPE/PVC wall + air core
    return make_match_from_spec(spec, conf, est_duct_eps, 0.02, host_attenuation_db_m, 0.001);
}

MaterialMatch MaterialDatabase::classify_anomaly(
    bool negative_metallic_polarity,
    double normalized_intensity,
    double host_epsr,
    double depth_m,
    double host_attenuation_db_m) {
    (void)depth_m;
    const double intensity = std::clamp(normalized_intensity, 0.0, 1.0);

    if (negative_metallic_polarity) {
        if (intensity >= 0.65) {
            const auto& spec = spec_for(MaterialKind::Conduit_Metallic);
            auto m = make_match_from_spec(
                spec, std::clamp(0.55 + 0.35 * intensity, 0.50, 0.94), 500.0, 5000.0, host_attenuation_db_m, 1.0);
            m.name_es = "Elemento metálico / Tapa enterrada";
            m.name_en = "Buried Metallic Object / Manhole Cover";
            m.estimated_velocity_m_ns = 0.0;
            return m;
        }
        // Moderate negative polarity -> localized water pocket / saturated lens (eps_anom > eps_host)
        const double est_eps = std::clamp(host_epsr * 2.4, 16.0, 80.0);
        const auto& spec = (est_eps > 40.0) ? spec_for(MaterialKind::Water_Fresh)
                                            : spec_for(MaterialKind::Clay_Wet);
        auto m = make_match_from_spec(
            spec, std::clamp(0.50 + 0.32 * intensity, 0.45, 0.88), est_eps, 45.0, host_attenuation_db_m * 1.5, 0.12);
        m.name_es = "Bolsa de humedad / Inclusión saturada";
        m.name_en = "Moisture Pocket / Saturated Inclusion";
        return m;
    }

    // Positive polarity (eps_anom < eps_host) -> subsurface air void / cavity or rock boulder
    if (intensity >= 0.55 && host_epsr >= 5.0) {
        const auto& spec = spec_for(MaterialKind::Air_Void);
        return make_match_from_spec(
            spec, std::clamp(0.54 + 0.34 * intensity, 0.48, 0.92), 1.1, 0.01, 0.05, 0.0005);
    }

    const auto& spec = spec_for(MaterialKind::Bedrock_Limestone_Granite);
    const double est_eps = std::clamp(host_epsr * 0.65, 4.5, 7.5);
    auto m = make_match_from_spec(
        spec, std::clamp(0.48 + 0.30 * intensity, 0.42, 0.86), est_eps, 1.0, host_attenuation_db_m * 0.6, 0.01);
    m.name_es = "Bloque rocoso / Canto rodado";
    m.name_en = "Rock Boulder / Stone Inclusion";
    return m;
}

}  // namespace gpr3d::geo

