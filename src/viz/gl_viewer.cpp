#include "gpr3d/viz/gl_viewer.hpp"
#include "gpr3d/geo/crs_transform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

#ifdef GPR3D_USE_OPENGL
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#endif

namespace gpr3d::viz {

#ifdef GPR3D_USE_OPENGL
namespace {

struct CameraState {
    double yaw_deg{-42.0};
    double pitch_deg{28.0};
    double distance{12.0};
    double target_x{0.0};
    double target_y{0.0};
    double target_z{-0.8};

    bool left_down{false};
    bool right_down{false};
    double last_mx{0.0};
    double last_my{0.0};

    std::size_t selected_trace{0};
    std::size_t total_traces{1};

    bool show_conduits{true};
    bool show_horizons{true};
    bool show_anomalies{true};
    bool show_points{true};
    bool show_ground{true};
};

void set_perspective(double fov_y_deg, double aspect, double z_near, double z_far) {
    const double f = 1.0 / std::tan(0.5 * fov_y_deg * (std::numbers::pi / 180.0));
    std::array<double, 16> m{};
    m[0] = f / std::max(0.05, aspect);
    m[5] = f;
    m[10] = (z_far + z_near) / (z_near - z_far);
    m[11] = -1.0;
    m[14] = (2.0 * z_far * z_near) / (z_near - z_far);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(m.data());
}

void draw_mesh_gl(const scene::Mesh3D& mesh, double z_scale) {
    if (mesh.empty()) {
        return;
    }
    const scene::Vec3 light_dir = scene::Vec3{0.35, -0.45, 0.82}.normalized();
    const bool has_normals = (mesh.normals.size() == mesh.vertices.size());

    glBegin(GL_TRIANGLES);
    for (const auto& tri : mesh.triangles) {
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t idx = tri[static_cast<std::size_t>(k)];
            const auto& v = mesh.vertices[idx];
            const auto n = has_normals ? mesh.normals[idx] : scene::Vec3{0.0, 0.0, 1.0};
            const double ndotl = std::abs(scene::Vec3::dot(n, light_dir));
            const float shade = static_cast<float>(0.42 + 0.58 * ndotl);

            glColor4f(mesh.color.r * shade, mesh.color.g * shade, mesh.color.b * shade, mesh.color.a);
            glVertex3d(v.x, v.y, v.z * z_scale);
        }
    }
    glEnd();
}

}  // namespace
#endif

bool GlSceneViewer::is_available() noexcept {
#ifdef GPR3D_USE_OPENGL
    return true;
#else
    return false;
#endif
}

void GlSceneViewer::run(const scene::Scene3D& scene, const ViewerConfig& cfg) {
#ifndef GPR3D_USE_OPENGL
    (void)scene;
    (void)cfg;
    throw std::runtime_error("gpr3d was built without OpenGL/GLFW support");
#else
    if (!glfwInit()) {
        throw std::runtime_error(
            "Failed to initialize GLFW (ensure an active X11/Wayland desktop display is available)");
    }

    GLFWwindow* window = glfwCreateWindow(
        cfg.window_width, cfg.window_height, cfg.window_title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create OpenGL window via GLFW");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Compute initial bounding box in ENU space
    double min_x = 1e9, min_y = 1e9, min_z = 1e9;
    double max_x = -1e9, max_y = -1e9, max_z = -1e9;
    auto expand_bounds = [&](const scene::Mesh3D& m) {
        for (const auto& v : m.vertices) {
            min_x = std::min(min_x, v.x); max_x = std::max(max_x, v.x);
            min_y = std::min(min_y, v.y); max_y = std::max(max_y, v.y);
            min_z = std::min(min_z, v.z); max_z = std::max(max_z, v.z);
        }
    };
    expand_bounds(scene.ground_surface);
    for (const auto& c : scene.conduits) expand_bounds(c.mesh);
    for (const auto& h : scene.horizons) expand_bounds(h.mesh);
    for (const auto& a : scene.anomalies) expand_bounds(a.mesh);
    if (min_x > max_x) {
        min_x = 0.0; max_x = 8.0;
        min_y = -1.0; max_y = 1.0;
        min_z = -2.0; max_z = 0.0;
    }

    CameraState cam{};
    cam.target_x = 0.5 * (min_x + max_x);
    cam.target_y = 0.5 * (min_y + max_y);
    cam.target_z = 0.5 * (min_z + max_z) * cfg.vertical_exaggeration;
    cam.distance = std::max(4.0, 1.35 * std::hypot(max_x - min_x, max_y - min_y));
    cam.total_traces = std::max<std::size_t>(1, scene.primary_processed_bscan.num_traces());
    cam.selected_trace = std::min(scene.fluctuations.representative_trace_idx, cam.total_traces - 1);

    const CameraState initial_cam = cam;
    glfwSetWindowUserPointer(window, &cam);

    glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int /*mods*/) {
        auto* c = static_cast<CameraState*>(glfwGetWindowUserPointer(w));
        if (button == GLFW_MOUSE_BUTTON_LEFT) c->left_down = (action == GLFW_PRESS);
        if (button == GLFW_MOUSE_BUTTON_RIGHT) c->right_down = (action == GLFW_PRESS);
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* w, double mx, double my) {
        auto* c = static_cast<CameraState*>(glfwGetWindowUserPointer(w));
        const double dx = mx - c->last_mx;
        const double dy = my - c->last_my;
        c->last_mx = mx;
        c->last_my = my;

        if (c->left_down) {
            c->yaw_deg += dx * 0.35;
            c->pitch_deg = std::clamp(c->pitch_deg + dy * 0.35, -85.0, 85.0);
        } else if (c->right_down) {
            const double pan = c->distance * 0.0015;
            const double rad = c->yaw_deg * (std::numbers::pi / 180.0);
            c->target_x -= (dx * std::cos(rad) + dy * std::sin(rad)) * pan;
            c->target_y -= (-dx * std::sin(rad) + dy * std::cos(rad)) * pan;
        }
    });

    glfwSetScrollCallback(window, [](GLFWwindow* w, double /*xoff*/, double yoff) {
        auto* c = static_cast<CameraState*>(glfwGetWindowUserPointer(w));
        c->distance = std::clamp(c->distance * (yoff > 0.0 ? 0.88 : 1.14), 1.0, 250.0);
    });

    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int /*sc*/, int action, int /*mods*/) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
        auto* c = static_cast<CameraState*>(glfwGetWindowUserPointer(w));
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_Q) {
            glfwSetWindowShouldClose(w, GLFW_TRUE);
        } else if (key == GLFW_KEY_LEFT && c->selected_trace > 0) {
            --c->selected_trace;
        } else if (key == GLFW_KEY_RIGHT && c->selected_trace + 1 < c->total_traces) {
            ++c->selected_trace;
        } else if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
            c->show_conduits = !c->show_conduits;
        } else if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
            c->show_horizons = !c->show_horizons;
        } else if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
            c->show_anomalies = !c->show_anomalies;
        } else if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
            c->show_points = !c->show_points;
        } else if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
            c->show_ground = !c->show_ground;
        }
    });

    std::cout << "=== gpr3d OpenGL Interactive Viewer Controls ===\n"
              << "  Mouse Left-Drag  : Orbit 3D scene\n"
              << "  Mouse Right-Drag : Pan 3D target\n"
              << "  Scroll Wheel     : Zoom in/out\n"
              << "  Left / Right Key : Move active GPR trace laser probe & inspect live A-scan\n"
              << "  Keys 1..5        : Toggle [1] Conduits [2] Horizons [3] Anomalies [4] Points [5] Ground\n"
              << "  Key R            : Reset camera\n"
              << "  ESC / Q          : Exit viewer\n";

    double max_bscan_amp = 1.0;
    for (const auto& tr : scene.primary_processed_bscan.traces) {
        for (double v : tr.samples) max_bscan_amp = std::max(max_bscan_amp, std::abs(v));
    }

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            cam.yaw_deg = initial_cam.yaw_deg;
            cam.pitch_deg = initial_cam.pitch_deg;
            cam.distance = initial_cam.distance;
            cam.target_x = initial_cam.target_x;
            cam.target_y = initial_cam.target_y;
            cam.target_z = initial_cam.target_z;
        }

        int fbw = 1, fbh = 1;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        const int hud_h = std::max(180, fbh / 4);
        const int view3d_h = std::max(100, fbh - hud_h);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // ---------------------------------------------------------------------
        // 1. Upper 3D Subsurface Scene Viewport
        // ---------------------------------------------------------------------
        glViewport(0, hud_h, fbw, view3d_h);
        glScissor(0, hud_h, fbw, view3d_h);
        glEnable(GL_SCISSOR_TEST);
        glClearColor(0.04f, 0.06f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        set_perspective(45.0, static_cast<double>(fbw) / static_cast<double>(view3d_h), 0.1, 1000.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslated(0.0, 0.0, -cam.distance);
        glRotated(-90.0 + cam.pitch_deg, 1.0, 0.0, 0.0);
        glRotated(cam.yaw_deg, 0.0, 0.0, 1.0);
        glTranslated(-cam.target_x, -cam.target_y, -cam.target_z);

        // Metric reference grid at z = 0
        glLineWidth(1.0f);
        glColor4f(0.18f, 0.24f, 0.34f, 0.55f);
        glBegin(GL_LINES);
        const double gx0 = std::floor(min_x - 1.0);
        const double gx1 = std::ceil(max_x + 1.0);
        const double gy0 = std::floor(min_y - 1.0);
        const double gy1 = std::ceil(max_y + 1.0);
        for (double x = gx0; x <= gx1; x += 1.0) {
            glVertex3d(x, gy0, 0.0); glVertex3d(x, gy1, 0.0);
        }
        for (double y = gy0; y <= gy1; y += 1.0) {
            glVertex3d(gx0, y, 0.0); glVertex3d(gx1, y, 0.0);
        }
        glEnd();

        // Draw opaque conduits & anomalies first
        if (cam.show_conduits) {
            for (const auto& c : scene.conduits) draw_mesh_gl(c.mesh, cfg.vertical_exaggeration);
        }
        if (cam.show_anomalies) {
            for (const auto& a : scene.anomalies) draw_mesh_gl(a.mesh, cfg.vertical_exaggeration);
        }

        // Draw 3D radar reflection point cloud
        if (cam.show_points && !scene.point_cloud.empty()) {
            glPointSize(3.0f);
            glBegin(GL_POINTS);
            for (const auto& pt : scene.point_cloud.points) {
                const float t = pt.intensity;
                glColor4f(0.15f + 0.85f * t, 0.75f * (1.0f - 0.3f * t), 1.0f - t, 0.80f);
                glVertex3d(pt.enu.east_m, pt.enu.north_m, pt.enu.up_m * cfg.vertical_exaggeration);
            }
            glEnd();
        }

        // Draw active vertical laser probe at selected trace
        if (!scene.primary_processed_bscan.empty() &&
            cam.selected_trace < scene.primary_processed_bscan.num_traces()) {
            const auto& tr = scene.primary_processed_bscan.traces[cam.selected_trace];
            if (tr.position_wgs84.has_value()) {
                const EnuCoord pe = geo::wgs84_to_enu(*tr.position_wgs84, scene.origin_wgs84);
                const double max_d = (!scene.fluctuations.depth_axis_m.empty())
                                         ? scene.fluctuations.depth_axis_m.back()
                                         : 2.0;
                glLineWidth(2.6f);
                glColor4f(0.20f, 0.88f, 1.0f, 0.95f);
                glBegin(GL_LINES);
                glVertex3d(pe.east_m, pe.north_m, (pe.up_m + 0.15) * cfg.vertical_exaggeration);
                glVertex3d(pe.east_m, pe.north_m, (pe.up_m - max_d) * cfg.vertical_exaggeration);
                glEnd();
            }
        }

        // Draw translucent horizons & ground surface last (depth mask read-only)
        glDepthMask(GL_FALSE);
        if (cam.show_horizons) {
            for (const auto& h : scene.horizons) draw_mesh_gl(h.mesh, cfg.vertical_exaggeration);
        }
        if (cam.show_ground) {
            draw_mesh_gl(scene.ground_surface, cfg.vertical_exaggeration);
        }
        glDepthMask(GL_TRUE);

        // ---------------------------------------------------------------------
        // 2. Lower 2D Signal Fluctuation HUD Strip (B-Scan Wiggle + Live A-Scan)
        // ---------------------------------------------------------------------
        glViewport(0, 0, fbw, hud_h);
        glScissor(0, 0, fbw, hud_h);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.07f, 0.09f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Top separator border
        glLineWidth(1.5f);
        glColor4f(0.24f, 0.32f, 0.44f, 1.0f);
        glBegin(GL_LINES);
        glVertex2d(0.0, 0.99); glVertex2d(1.0, 0.99);
        glVertex2d(0.50, 0.05); glVertex2d(0.50, 0.95);
        glEnd();

        // Left HUD [0.02 .. 0.48]: B-Scan Wiggle Fluctuation Strip + Probe Cursor
        const auto& bscan = scene.primary_processed_bscan;
        if (!bscan.empty() && bscan.num_samples() > 2) {
            const std::size_t n_tr = bscan.num_traces();
            const std::size_t ns = bscan.num_samples();
            const std::size_t w_cnt = std::min<std::size_t>(42, n_tr);
            const double slot_w = 0.46 / static_cast<double>(w_cnt);

            glLineWidth(1.0f);
            glColor4f(0.55f, 0.78f, 0.96f, 0.80f);
            for (std::size_t k = 0; k < w_cnt; ++k) {
                const std::size_t ti = (k * (n_tr - 1)) / std::max<std::size_t>(1, w_cnt - 1);
                const double x0 = 0.02 + (static_cast<double>(k) + 0.5) * slot_w;
                glBegin(GL_LINE_STRIP);
                for (std::size_t j = 0; j < ns; j += 2) {
                    const double y = 0.92 - (static_cast<double>(j) / static_cast<double>(ns - 1)) * 0.84;
                    const double dx = (bscan.traces[ti].samples[j] / max_bscan_amp) * slot_w * 1.2;
                    glVertex2d(x0 + dx, y);
                }
                glEnd();
            }

            const double cx = 0.02 + (static_cast<double>(cam.selected_trace) /
                                      static_cast<double>(std::max<std::size_t>(1, n_tr - 1))) * 0.46;
            glLineWidth(2.2f);
            glColor4f(0.20f, 0.90f, 1.0f, 1.0f);
            glBegin(GL_LINES);
            glVertex2d(cx, 0.06); glVertex2d(cx, 0.94);
            glEnd();
        }

        // Right HUD [0.52 .. 0.98]: Live Selected Trace A-Scan Waveform + Hilbert Envelope
        if (!bscan.empty() && cam.selected_trace < bscan.num_traces()) {
            const auto& tr = bscan.traces[cam.selected_trace];
            const std::size_t ns = tr.samples.size();
            double local_max = 1e-9;
            for (double e : tr.envelope) local_max = std::max(local_max, e);
            for (double s : tr.samples) local_max = std::max(local_max, std::abs(s));

            // Center axis
            glLineWidth(1.0f);
            glColor4f(0.25f, 0.32f, 0.42f, 0.8f);
            glBegin(GL_LINES);
            glVertex2d(0.52, 0.50); glVertex2d(0.98, 0.50);
            glEnd();

            // +- Hilbert Envelope
            if (tr.envelope.size() == ns) {
                glLineWidth(1.5f);
                glColor4f(0.98f, 0.72f, 0.16f, 0.92f);
                for (int sg : {1, -1}) {
                    glBegin(GL_LINE_STRIP);
                    for (std::size_t j = 0; j < ns; ++j) {
                        const double x = 0.52 + (static_cast<double>(j) / static_cast<double>(ns - 1)) * 0.46;
                        const double y = 0.50 + static_cast<double>(sg) * (tr.envelope[j] / local_max) * 0.40;
                        glVertex2d(x, y);
                    }
                    glEnd();
                }
            }

            // Oscillating Radar Signal s(t)
            glLineWidth(1.6f);
            glColor4f(0.18f, 0.90f, 0.98f, 1.0f);
            glBegin(GL_LINE_STRIP);
            for (std::size_t j = 0; j < ns; ++j) {
                const double x = 0.52 + (static_cast<double>(j) / static_cast<double>(ns - 1)) * 0.46;
                const double y = 0.50 + (tr.samples[j] / local_max) * 0.40;
                glVertex2d(x, y);
            }
            glEnd();
        }

        glDisable(GL_SCISSOR_TEST);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
#endif
}

}  // namespace gpr3d::viz

