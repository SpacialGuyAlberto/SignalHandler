#pragma once

#include "gpr3d/scene/objects_3d.hpp"

#include <string>

namespace gpr3d::viz {

struct ViewerConfig {
    int window_width{1360};
    int window_height{820};
    std::string window_title{"gpr3d - Interactive 3D Subsurface Objects & Signal Fluctuations"};
    double vertical_exaggeration{1.8};
};

/// Native desktop OpenGL + GLFW interactive viewer for 3D GPR objects and live waveform fluctuations.
class GlSceneViewer {
public:
    /// Returns true if compiled with OpenGL + GLFW3 support.
    [[nodiscard]] static bool is_available() noexcept;

    /// Launch interactive 3D + 2D fluctuation window (blocks until user closes window).
    static void run(const scene::Scene3D& scene, const ViewerConfig& cfg = {});
};

}  // namespace gpr3d::viz

