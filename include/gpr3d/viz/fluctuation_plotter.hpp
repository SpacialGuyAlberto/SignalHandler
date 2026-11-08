#pragma once

#include "gpr3d/scene/objects_3d.hpp"

#include <filesystem>

namespace gpr3d::viz {

struct PlotOptions {
    int width_px{1600};
    int height_px{1040};
    std::size_t wiggle_trace_count{36};
};

/// Generates static high-resolution graphical fluctuation charts (PNG and vector SVG via Cairo)
/// and self-contained interactive 3D + 2D waveform fluctuation HTML dashboards.
class FluctuationPlotter {
public:
    /// Render 4-panel geophysical fluctuation figure (B-scan heatmap + objects, Wiggle plot,
    /// A-scan waveform/envelope/phase, and FFT spectrum + spatial RMS) to PNG.
    static void write_png(
        const std::filesystem::path& png_path,
        const scene::Scene3D& scene,
        const PlotOptions& opts = {});

    /// Render the same 4-panel geophysical fluctuation figure to scalable vector SVG.
    static void write_svg(
        const std::filesystem::path& svg_path,
        const scene::Scene3D& scene,
        const PlotOptions& opts = {});

    /// Generate a zero-dependency standalone HTML5 interactive 3D object & signal fluctuation dashboard.
    static void write_html_dashboard(
        const std::filesystem::path& html_path,
        const scene::Scene3D& scene);
};

}  // namespace gpr3d::viz

