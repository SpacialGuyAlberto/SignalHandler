#include "gpr3d/viz/fluctuation_plotter.hpp"
#include "gpr3d/geo/crs_transform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef GPR3D_USE_CAIRO
#include <cairo/cairo-svg.h>
#include <cairo/cairo.h>
#endif

namespace gpr3d::viz {

namespace {

#ifdef GPR3D_USE_CAIRO
struct Rect {
    double x{};
    double y{};
    double w{};
    double h{};
};

/// Diverging geophysical radar colormap (v in [-1, 1]): deep blue -> slate dark/white -> crimson/amber.
std::array<std::uint8_t, 3> seismic_diverging_rgb(double v) noexcept {
    v = std::clamp(v, -1.0, 1.0);
    double r = 0.0, g = 0.0, b = 0.0;
    if (v < 0.0) {
        const double t = -v;
        r = 0.94 * (1.0 - t) + 0.06 * t;
        g = 0.95 * (1.0 - t) + 0.36 * t;
        b = 0.97 * (1.0 - t) + 0.92 * t;
    } else {
        const double t = v;
        r = 0.94 * (1.0 - t) + 0.92 * t;
        g = 0.95 * (1.0 - t) + 0.16 * t;
        b = 0.97 * (1.0 - t) + 0.14 * t;
    }
    return {
        static_cast<std::uint8_t>(std::clamp(std::lround(r * 255.0), 0L, 255L)),
        static_cast<std::uint8_t>(std::clamp(std::lround(g * 255.0), 0L, 255L)),
        static_cast<std::uint8_t>(std::clamp(std::lround(b * 255.0), 0L, 255L))
    };
}

void draw_panel_frame(cairo_t* cr, const Rect& r, const std::string& title, const std::string& subtitle) {
    // Card background
    cairo_set_source_rgb(cr, 0.09, 0.11, 0.15);
    cairo_rectangle(cr, r.x, r.y, r.w, r.h);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.22, 0.27, 0.35);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);

    // Title text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14.0);
    cairo_set_source_rgb(cr, 0.92, 0.95, 0.99);
    cairo_move_to(cr, r.x + 14.0, r.y + 22.0);
    cairo_show_text(cr, title.c_str());

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.62, 0.70, 0.80);
    cairo_move_to(cr, r.x + 14.0, r.y + 38.0);
    cairo_show_text(cr, subtitle.c_str());
}

void render_4panel_cairo(cairo_t* cr, const scene::Scene3D& scene, const PlotOptions& opts) {
    const double W = static_cast<double>(opts.width_px);
    const double H = static_cast<double>(opts.height_px);

    // Dark dashboard background
    cairo_set_source_rgb(cr, 0.05, 0.07, 0.10);
    cairo_rectangle(cr, 0.0, 0.0, W, H);
    cairo_fill(cr);

    // Global header banner
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20.0);
    cairo_set_source_rgb(cr, 0.96, 0.98, 1.00);
    cairo_move_to(cr, 24.0, 32.0);
    cairo_show_text(cr, "GPR / GDR Subsurface Signal Fluctuations & 3D Object Interpretation");

    {
        std::ostringstream info;
        info << std::fixed << std::setprecision(6);
        if (scene.has_valid_crs) {
            info << "Origin WGS84: (" << scene.origin_wgs84.latitude_deg << ", "
                 << scene.origin_wgs84.longitude_deg << ", "
                 << std::setprecision(2) << scene.origin_wgs84.altitude_m << " m)  |  UTM "
                 << scene.origin_utm.zone_string() << " (EPSG:" << scene.origin_utm.epsg_code() << ")  |  ";
        }
        info << "Detected 3D Objects: " << scene.conduits.size() << " Conduits, "
             << scene.horizons.size() << " Horizons, "
             << scene.anomalies.size() << " Anomalies";

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.0);
        cairo_set_source_rgb(cr, 0.56, 0.76, 0.96);
        cairo_move_to(cr, 24.0, 52.0);
        cairo_show_text(cr, info.str().c_str());
    }

    const double pad = 20.0;
    const double top_y = 68.0;
    const double panel_w = (W - 3.0 * pad) * 0.5;
    const double panel_h = (H - top_y - 2.0 * pad) * 0.5;

    const Rect p1{pad, top_y, panel_w, panel_h};
    const Rect p2{2.0 * pad + panel_w, top_y, panel_w, panel_h};
    const Rect p3{pad, top_y + panel_h + pad, panel_w, panel_h};
    const Rect p4{2.0 * pad + panel_w, top_y + panel_h + pad, panel_w, panel_h};

    const auto& bscan = scene.primary_processed_bscan;
    const auto& fluc = scene.fluctuations;

    // -------------------------------------------------------------------------
    // PANEL 1: B-Scan Radargram Heatmap + Detected 3D Objects Overlay
    // -------------------------------------------------------------------------
    draw_panel_frame(
        cr, p1,
        "1. B-Scan Radargram & Detected 3D Subsurface Objects",
        "Signed Wavelet Amplitude Heatmap + Horizons (dashed) + Conduits (crosshairs)");

    const Rect c1{p1.x + 54.0, p1.y + 50.0, p1.w - 72.0, p1.h - 78.0};
    if (!bscan.empty() && bscan.num_samples() > 1) {
        const int img_w = std::max(16, static_cast<int>(c1.w));
        const int img_h = std::max(16, static_cast<int>(c1.h));

        double max_abs = 1e-9;
        for (const auto& tr : bscan.traces) {
            for (double s : tr.samples) {
                max_abs = std::max(max_abs, std::abs(s));
            }
        }
        const double clip_amp = 0.65 * max_abs;  // Enhance contrast of subsurface oscillations

        cairo_surface_t* img = cairo_image_surface_create(CAIRO_FORMAT_RGB24, img_w, img_h);
        unsigned char* data = cairo_image_surface_get_data(img);
        const int stride = cairo_image_surface_get_stride(img);
        cairo_surface_flush(img);

        const std::size_t n_tr = bscan.num_traces();
        const std::size_t ns = bscan.num_samples();

        for (int py = 0; py < img_h; ++py) {
            const double u_y = static_cast<double>(py) / static_cast<double>(img_h - 1);
            const auto j = std::min(ns - 1, static_cast<std::size_t>(u_y * static_cast<double>(ns - 1)));
            auto* row = reinterpret_cast<std::uint32_t*>(data + py * stride);

            for (int px = 0; px < img_w; ++px) {
                const double u_x = static_cast<double>(px) / static_cast<double>(img_w - 1);
                const auto i = std::min(n_tr - 1, static_cast<std::size_t>(u_x * static_cast<double>(n_tr - 1)));
                const double val = bscan.traces[i].samples[j] / clip_amp;
                const auto rgb = seismic_diverging_rgb(val);
                row[px] = (static_cast<std::uint32_t>(rgb[0]) << 16U) |
                          (static_cast<std::uint32_t>(rgb[1]) << 8U) |
                          static_cast<std::uint32_t>(rgb[2]);
            }
        }
        cairo_surface_mark_dirty(img);

        cairo_save(cr);
        cairo_translate(cr, c1.x, c1.y);
        cairo_set_source_surface(cr, img, 0.0, 0.0);
        cairo_paint(cr);
        cairo_restore(cr);
        cairo_surface_destroy(img);

        const double max_x_m = std::max(0.01, bscan.traces.back().distance_along_track_m);
        const double max_d_m = (!fluc.depth_axis_m.empty()) ? std::max(0.01, fluc.depth_axis_m.back()) : 2.0;

        // Overlay tracked stratigraphic horizons
        cairo_save(cr);
        cairo_rectangle(cr, c1.x, c1.y, c1.w, c1.h);
        cairo_clip(cr);

        const double dashes[2] = {6.0, 4.0};
        cairo_set_dash(cr, dashes, 2, 0.0);
        cairo_set_line_width(cr, 2.2);
        for (const auto& hz : scene.horizons) {
            cairo_set_source_rgb(cr, 1.0, 0.78, 0.12);
            const double x0 = c1.x;
            const double x1 = c1.x + c1.w;
            const double d0 = hz.mean_depth_m - 0.5 * hz.dip_slope_m_per_m * max_x_m;
            const double d1 = hz.mean_depth_m + 0.5 * hz.dip_slope_m_per_m * max_x_m;
            cairo_move_to(cr, x0, c1.y + (d0 / max_d_m) * c1.h);
            cairo_line_to(cr, x1, c1.y + (d1 / max_d_m) * c1.h);
            cairo_stroke(cr);
        }
        cairo_set_dash(cr, nullptr, 0, 0.0);

        // Overlay detected conduits projected along track
        for (const auto& cond : scene.conduits) {
            if (cond.centerline_wgs84.empty()) continue;
            // Find closest trace along primary B-scan to conduit's first node
            std::size_t best_i = 0;
            double best_dist = 1e9;
            for (std::size_t i = 0; i < bscan.num_traces(); ++i) {
                const double d = geo::horizontal_distance_m(
                    *bscan.traces[i].position_wgs84, cond.centerline_wgs84.front());
                if (d < best_dist) {
                    best_dist = d;
                    best_i = i;
                }
            }
            const double cx = c1.x + (bscan.traces[best_i].distance_along_track_m / max_x_m) * c1.w;
            const double cy = c1.y + (cond.mean_depth_m / max_d_m) * c1.h;

            if (cond.polarity == scene::PolarityKind::MetallicOrHighPermittivity) {
                cairo_set_source_rgb(cr, 0.98, 0.22, 0.20);
            } else {
                cairo_set_source_rgb(cr, 0.08, 0.92, 0.62);
            }
            cairo_set_line_width(cr, 2.4);
            cairo_arc(cr, cx, cy, 8.5, 0.0, 2.0 * std::numbers::pi);
            cairo_stroke(cr);
            cairo_move_to(cr, cx - 12.0, cy); cairo_line_to(cr, cx + 12.0, cy);
            cairo_move_to(cr, cx, cy - 12.0); cairo_line_to(cr, cx, cy + 12.0);
            cairo_stroke(cr);
        }

        // Probe line at representative trace
        if (fluc.representative_trace_idx < n_tr) {
            const double px = c1.x +
                              (bscan.traces[fluc.representative_trace_idx].distance_along_track_m / max_x_m) * c1.w;
            cairo_set_source_rgba(cr, 0.20, 0.88, 1.0, 0.85);
            cairo_set_line_width(cr, 1.4);
            cairo_move_to(cr, px, c1.y);
            cairo_line_to(cr, px, c1.y + c1.h);
            cairo_stroke(cr);
        }
        cairo_restore(cr);

        // Axis ticks
        cairo_set_source_rgb(cr, 0.72, 0.78, 0.86);
        cairo_set_font_size(cr, 10.5);
        for (int k = 0; k <= 4; ++k) {
            const double u = static_cast<double>(k) / 4.0;
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "%.2fm", u * max_d_m);
            cairo_move_to(cr, p1.x + 8.0, c1.y + u * c1.h + 4.0);
            cairo_show_text(cr, lbl);

            std::snprintf(lbl, sizeof(lbl), "%.1fm", u * max_x_m);
            cairo_move_to(cr, c1.x + u * c1.w - 14.0, c1.y + c1.h + 16.0);
            cairo_show_text(cr, lbl);
        }
    }

    // -------------------------------------------------------------------------
    // PANEL 2: Geophysical Wiggle-Trace Fluctuation Plot
    // -------------------------------------------------------------------------
    draw_panel_frame(
        cr, p2,
        "2. Geophysical Wiggle-Trace Signal Fluctuations",
        "Decimated Vertical Wavelets with Positive-Polarity Excursion Shading");

    const Rect c2{p2.x + 50.0, p2.y + 50.0, p2.w - 68.0, p2.h - 78.0};
    cairo_set_source_rgb(cr, 0.05, 0.07, 0.11);
    cairo_rectangle(cr, c2.x, c2.y, c2.w, c2.h);
    cairo_fill(cr);

    if (!bscan.empty() && bscan.num_samples() > 2) {
        const std::size_t n_tr = bscan.num_traces();
        const std::size_t ns = bscan.num_samples();
        const std::size_t w_count = std::min(opts.wiggle_trace_count, n_tr);
        const double dx_slot = c2.w / static_cast<double>(w_count);

        double max_abs = 1e-9;
        for (const auto& tr : bscan.traces) {
            for (double v : tr.samples) max_abs = std::max(max_abs, std::abs(v));
        }

        cairo_save(cr);
        cairo_rectangle(cr, c2.x, c2.y, c2.w, c2.h);
        cairo_clip(cr);

        for (std::size_t k = 0; k < w_count; ++k) {
            const std::size_t ti = (k * (n_tr - 1)) / std::max<std::size_t>(1, w_count - 1);
            const auto& samples = bscan.traces[ti].samples;
            const double x_base = c2.x + (static_cast<double>(k) + 0.5) * dx_slot;
            const double amp_scale = 2.4 * dx_slot / max_abs;

            // Positive lobe shading
            cairo_set_source_rgba(cr, 0.16, 0.78, 0.96, 0.48);
            cairo_move_to(cr, x_base, c2.y);
            for (std::size_t j = 0; j < ns; ++j) {
                const double y = c2.y + (static_cast<double>(j) / static_cast<double>(ns - 1)) * c2.h;
                const double pos_deflect = std::max(0.0, samples[j]) * amp_scale;
                cairo_line_to(cr, x_base + pos_deflect, y);
            }
            cairo_line_to(cr, x_base, c2.y + c2.h);
            cairo_close_path(cr);
            cairo_fill(cr);

            // Full wiggle waveform stroke
            cairo_set_source_rgba(cr, 0.82, 0.90, 0.98, 0.85);
            cairo_set_line_width(cr, 0.9);
            for (std::size_t j = 0; j < ns; ++j) {
                const double y = c2.y + (static_cast<double>(j) / static_cast<double>(ns - 1)) * c2.h;
                const double x = x_base + samples[j] * amp_scale;
                if (j == 0) cairo_move_to(cr, x, y);
                else cairo_line_to(cr, x, y);
            }
            cairo_stroke(cr);
        }
        cairo_restore(cr);

        const double max_d_m = (!fluc.depth_axis_m.empty()) ? std::max(0.01, fluc.depth_axis_m.back()) : 2.0;
        const double max_x_m = std::max(0.01, bscan.traces.back().distance_along_track_m);
        cairo_set_source_rgb(cr, 0.72, 0.78, 0.86);
        cairo_set_font_size(cr, 10.5);
        for (int k = 0; k <= 4; ++k) {
            const double u = static_cast<double>(k) / 4.0;
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "%.2fm", u * max_d_m);
            cairo_move_to(cr, p2.x + 8.0, c2.y + u * c2.h + 4.0);
            cairo_show_text(cr, lbl);

            std::snprintf(lbl, sizeof(lbl), "%.1fm", u * max_x_m);
            cairo_move_to(cr, c2.x + u * c2.w - 14.0, c2.y + c2.h + 16.0);
            cairo_show_text(cr, lbl);
        }
    }

    // -------------------------------------------------------------------------
    // PANEL 3: Representative A-Scan Waveform, Hilbert Envelope & Phase
    // -------------------------------------------------------------------------
    {
        std::ostringstream sub;
        sub << "Trace #" << fluc.representative_trace_idx
            << "  |  Cyan: Signal s(t), Amber: +-Hilbert Envelope |z(t)|";
        draw_panel_frame(cr, p3, "3. A-Scan Waveform & Instantaneous Envelope Fluctuations", sub.str());
    }

    const Rect c3{p3.x + 54.0, p3.y + 50.0, p3.w - 72.0, p3.h - 78.0};
    cairo_set_source_rgb(cr, 0.05, 0.07, 0.11);
    cairo_rectangle(cr, c3.x, c3.y, c3.w, c3.h);
    cairo_fill(cr);

    if (fluc.rep_waveform.size() > 2) {
        const std::size_t ns = fluc.rep_waveform.size();
        double max_env = 1e-9;
        for (double e : fluc.rep_envelope) max_env = std::max(max_env, e);
        for (double s : fluc.rep_waveform) max_env = std::max(max_env, std::abs(s));

        const double mid_y = c3.y + 0.5 * c3.h;

        // Zero line
        cairo_set_source_rgba(cr, 0.35, 0.42, 0.52, 0.6);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, c3.x, mid_y);
        cairo_line_to(cr, c3.x + c3.w, mid_y);
        cairo_stroke(cr);

        // +Envelope and -Envelope
        cairo_set_source_rgba(cr, 0.98, 0.72, 0.18, 0.90);
        cairo_set_line_width(cr, 1.5);
        for (int sign : {1, -1}) {
            for (std::size_t j = 0; j < ns; ++j) {
                const double x = c3.x + (static_cast<double>(j) / static_cast<double>(ns - 1)) * c3.w;
                const double y = mid_y - static_cast<double>(sign) * (fluc.rep_envelope[j] / max_env) * (0.45 * c3.h);
                if (j == 0) cairo_move_to(cr, x, y);
                else cairo_line_to(cr, x, y);
            }
            cairo_stroke(cr);
        }

        // Waveform s(t)
        cairo_set_source_rgb(cr, 0.18, 0.88, 0.98);
        cairo_set_line_width(cr, 1.4);
        for (std::size_t j = 0; j < ns; ++j) {
            const double x = c3.x + (static_cast<double>(j) / static_cast<double>(ns - 1)) * c3.w;
            const double y = mid_y - (fluc.rep_waveform[j] / max_env) * (0.45 * c3.h);
            if (j == 0) cairo_move_to(cr, x, y);
            else cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);

        const double max_d_m = (!fluc.depth_axis_m.empty()) ? fluc.depth_axis_m.back() : 2.0;
        cairo_set_source_rgb(cr, 0.72, 0.78, 0.86);
        cairo_set_font_size(cr, 10.5);
        for (int k = 0; k <= 4; ++k) {
            const double u = static_cast<double>(k) / 4.0;
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "%.2fm", u * max_d_m);
            cairo_move_to(cr, c3.x + u * c3.w - 14.0, c3.y + c3.h + 16.0);
            cairo_show_text(cr, lbl);
        }
    }

    // -------------------------------------------------------------------------
    // PANEL 4: FFT Power Spectrum (dB) & Per-Trace RMS Spatial Fluctuation
    // -------------------------------------------------------------------------
    draw_panel_frame(
        cr, p4,
        "4. Spectral Power Density (FFT) & Spatial Energy Fluctuation",
        "Top: Ensemble Spectrum [0..1600 MHz] (dB)  |  Bottom: Per-Trace RMS Energy");

    const Rect c4_top{p4.x + 54.0, p4.y + 50.0, p4.w - 72.0, (p4.h - 92.0) * 0.56};
    const Rect c4_bot{p4.x + 54.0, c4_top.y + c4_top.h + 24.0, p4.w - 72.0, (p4.h - 92.0) * 0.40};

    cairo_set_source_rgb(cr, 0.05, 0.07, 0.11);
    cairo_rectangle(cr, c4_top.x, c4_top.y, c4_top.w, c4_top.h);
    cairo_rectangle(cr, c4_bot.x, c4_bot.y, c4_bot.w, c4_bot.h);
    cairo_fill(cr);

    if (fluc.freq_axis_mhz.size() > 2) {
        const double max_plot_mhz = std::min(1600.0, fluc.freq_axis_mhz.back());
        constexpr double min_db = -50.0;

        cairo_save(cr);
        cairo_rectangle(cr, c4_top.x, c4_top.y, c4_top.w, c4_top.h);
        cairo_clip(cr);

        cairo_set_source_rgba(cr, 0.58, 0.36, 0.98, 0.35);
        cairo_move_to(cr, c4_top.x, c4_top.y + c4_top.h);
        for (std::size_t k = 0; k < fluc.freq_axis_mhz.size(); ++k) {
            if (fluc.freq_axis_mhz[k] > max_plot_mhz) break;
            const double x = c4_top.x + (fluc.freq_axis_mhz[k] / max_plot_mhz) * c4_top.w;
            const double db = std::clamp(fluc.mean_psd_db[k], min_db, 0.0);
            const double y = c4_top.y + (db / min_db) * c4_top.h;
            cairo_line_to(cr, x, y);
        }
        cairo_line_to(cr, c4_top.x + c4_top.w, c4_top.y + c4_top.h);
        cairo_close_path(cr);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 0.72, 0.54, 1.0);
        cairo_set_line_width(cr, 1.8);
        bool started = false;
        for (std::size_t k = 0; k < fluc.freq_axis_mhz.size(); ++k) {
            if (fluc.freq_axis_mhz[k] > max_plot_mhz) break;
            const double x = c4_top.x + (fluc.freq_axis_mhz[k] / max_plot_mhz) * c4_top.w;
            const double db = std::clamp(fluc.mean_psd_db[k], min_db, 0.0);
            const double y = c4_top.y + (db / min_db) * c4_top.h;
            if (!started) {
                cairo_move_to(cr, x, y);
                started = true;
            } else {
                cairo_line_to(cr, x, y);
            }
        }
        cairo_stroke(cr);
        cairo_restore(cr);

        cairo_set_source_rgb(cr, 0.72, 0.78, 0.86);
        cairo_set_font_size(cr, 10.0);
        for (int k = 0; k <= 4; ++k) {
            const double u = static_cast<double>(k) / 4.0;
            char lbl[32];
            std::snprintf(lbl, sizeof(lbl), "%.0f MHz", u * max_plot_mhz);
            cairo_move_to(cr, c4_top.x + u * c4_top.w - 18.0, c4_top.y + c4_top.h + 12.0);
            cairo_show_text(cr, lbl);
        }
        cairo_move_to(cr, p4.x + 12.0, c4_top.y + 10.0); cairo_show_text(cr, "  0dB");
        cairo_move_to(cr, p4.x + 12.0, c4_top.y + c4_top.h); cairo_show_text(cr, "-50dB");
    }

    if (fluc.per_trace.size() > 1) {
        double min_rms = fluc.per_trace.front().rms_amplitude;
        double max_rms = min_rms;
        for (const auto& m : fluc.per_trace) {
            min_rms = std::min(min_rms, m.rms_amplitude);
            max_rms = std::max(max_rms, m.rms_amplitude);
        }
        const double span = std::max(1e-9, max_rms - min_rms);

        cairo_set_source_rgb(cr, 0.16, 0.92, 0.58);
        cairo_set_line_width(cr, 1.6);
        for (std::size_t i = 0; i < fluc.per_trace.size(); ++i) {
            const double u = static_cast<double>(i) / static_cast<double>(fluc.per_trace.size() - 1);
            const double v = (fluc.per_trace[i].rms_amplitude - min_rms) / span;
            const double x = c4_bot.x + u * c4_bot.w;
            const double y = c4_bot.y + c4_bot.h - (0.1 + 0.8 * v) * c4_bot.h;
            if (i == 0) cairo_move_to(cr, x, y);
            else cairo_line_to(cr, x, y);
        }
        cairo_stroke(cr);

        cairo_set_source_rgb(cr, 0.72, 0.78, 0.86);
        cairo_set_font_size(cr, 10.0);
        cairo_move_to(cr, p4.x + 12.0, c4_bot.y + 12.0);
        cairo_show_text(cr, "RMS");
    }
}
#endif

}  // namespace

void FluctuationPlotter::write_png(
    const std::filesystem::path& png_path,
    const scene::Scene3D& scene,
    const PlotOptions& opts) {
#ifdef GPR3D_USE_CAIRO
    cairo_surface_t* surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, opts.width_px, opts.height_px);
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        throw std::runtime_error("Cairo failed to allocate image surface");
    }
    cairo_t* cr = cairo_create(surface);
    render_4panel_cairo(cr, scene, opts);
    cairo_destroy(cr);

    const cairo_status_t st = cairo_surface_write_to_png(surface, png_path.string().c_str());
    cairo_surface_destroy(surface);
    if (st != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error("Cairo failed writing PNG file: " + png_path.string());
    }
#else
    (void)scene;
    (void)opts;
    throw std::runtime_error("PNG export requires Cairo support enabled at build time: " + png_path.string());
#endif
}

void FluctuationPlotter::write_svg(
    const std::filesystem::path& svg_path,
    const scene::Scene3D& scene,
    const PlotOptions& opts) {
#ifdef GPR3D_USE_CAIRO
    cairo_surface_t* surface = cairo_svg_surface_create(
        svg_path.string().c_str(),
        static_cast<double>(opts.width_px),
        static_cast<double>(opts.height_px));
    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        throw std::runtime_error("Cairo failed to create SVG surface: " + svg_path.string());
    }
    cairo_t* cr = cairo_create(surface);
    render_4panel_cairo(cr, scene, opts);
    cairo_destroy(cr);
    cairo_surface_finish(surface);
    cairo_surface_destroy(surface);
#else
    (void)scene;
    (void)opts;
    throw std::runtime_error("SVG export requires Cairo support enabled at build time: " + svg_path.string());
#endif
}

void FluctuationPlotter::write_html_dashboard(
    const std::filesystem::path& html_path,
    const scene::Scene3D& scene) {
    std::ofstream out(html_path, std::ios::out | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open HTML dashboard for writing: " + html_path.string());
    }

    // Subsample B-scan matrix slightly if huge so standalone HTML stays fast & lightweight
    const auto& bscan = scene.primary_processed_bscan;
    const std::size_t n_tr = bscan.num_traces();
    const std::size_t ns = bscan.num_samples();
    const std::size_t tr_step = std::max<std::size_t>(1, n_tr / 180);
    const std::size_t s_step = std::max<std::size_t>(1, ns / 220);

    auto emit_mesh_json = [&](const scene::Mesh3D& m, const std::string& category, const std::string& meta_info) {
        out << "{\"name\":\"" << m.name << "\",\"cat\":\"" << category << "\",\"info\":\"" << meta_info << "\","
            << std::fixed << std::setprecision(3)
            << "\"rgba\":[" << m.color.r << "," << m.color.g << "," << m.color.b << "," << m.color.a << "],"
            << "\"v\":[";
        for (std::size_t i = 0; i < m.vertices.size(); ++i) {
            if (i > 0) out << ",";
            out << "[" << m.vertices[i].x << "," << m.vertices[i].y << "," << m.vertices[i].z << "]";
        }
        out << "],\"f\":[";
        for (std::size_t i = 0; i < m.triangles.size(); ++i) {
            if (i > 0) out << ",";
            out << "[" << m.triangles[i][0] << "," << m.triangles[i][1] << "," << m.triangles[i][2] << "]";
        }
        out << "]}";
    };

    out << R"HTML(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<title>gpr3d - Escena Subterránea 3D & Fluctuaciones de Señal</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #0b0f17; color: #e2e8f0; font-family: system-ui, -apple-system, sans-serif; height: 100vh; display: flex; flex-direction: column; overflow: hidden; }
  header { padding: 10px 18px; background: #111827; border-bottom: 1px solid #1f2937; display: flex; justify-content: space-between; align-items: center; }
  header h1 { font-size: 16px; font-weight: 700; color: #f8fafc; }
  header .crs { font-size: 12px; color: #60a5fa; font-family: monospace; }
  .layout { flex: 1; display: grid; grid-template-columns: 58% 42%; gap: 10px; padding: 10px; min-height: 0; }
  .card { background: #111827; border: 1px solid #1f2937; border-radius: 8px; display: flex; flex-direction: column; overflow: hidden; position: relative; }
  .card-hdr { padding: 8px 12px; background: #161f30; border-bottom: 1px solid #1f2937; font-size: 12px; font-weight: 600; display: flex; justify-content: space-between; align-items: center; }
  .controls { display: flex; gap: 10px; font-size: 11px; font-weight: 400; color: #cbd5e1; }
  .controls label { cursor: pointer; display: flex; align-items: center; gap: 4px; }
  .right-col { display: grid; grid-template-rows: 1fr 1fr; gap: 10px; min-height: 0; }
  canvas { width: 100%; height: 100%; display: block; }
  .obj-overlay { position: absolute; bottom: 10px; left: 10px; background: rgba(15,23,42,0.88); border: 1px solid #334155; border-radius: 6px; padding: 8px 10px; max-width: 380px; max-height: 170px; overflow-y: auto; font-size: 11px; line-height: 1.45; }
  .badge { display: inline-block; width: 9px; height: 9px; border-radius: 50%; margin-right: 5px; }
</style>
</head>
<body>
<header>
  <h1>gpr3d — Interpretación de Objetos 3D &amp; Fluctuaciones Gráficas GPR</h1>
  <div class="crs" id="crsBanner"></div>
</header>
<div class="layout">
  <div class="card">
    <div class="card-hdr">
      <span>Escena 3D Georreferenciada (Arrastrar: Orbitar | Rueda: Zoom)</span>
      <div class="controls">
        <label><input type="checkbox" id="chkConduits" checked> Conductos</label>
        <label><input type="checkbox" id="chkHorizons" checked> Estratos</label>
        <label><input type="checkbox" id="chkAnomalies" checked> Anomalías</label>
        <label><input type="checkbox" id="chkPoints" checked> Nube 3D</label>
        <label><input type="checkbox" id="chkGround" checked> Superficie</label>
      </div>
    </div>
    <canvas id="cv3d"></canvas>
    <div class="obj-overlay" id="objList"></div>
  </div>
  <div class="right-col">
    <div class="card">
      <div class="card-hdr">
        <span>Radargrama B-Scan &amp; Fluctuación (Haz clic para inspeccionar traza)</span>
        <span id="traceInfo" style="color:#38bdf8;font-family:monospace;"></span>
      </div>
      <canvas id="cvBscan"></canvas>
    </div>
    <div class="card">
      <div class="card-hdr">
        <span>Fluctuación A-Scan s(t), Envolvente de Hilbert |z(t)| y Espectro FFT</span>
      </div>
      <canvas id="cvAscan"></canvas>
    </div>
  </div>
</div>
<script>
const SCENE = )HTML";

    // Embed compact JSON payload
    out << "{\"crs\":{";
    if (scene.has_valid_crs) {
        out << std::fixed << std::setprecision(7)
            << "\"lat\":" << scene.origin_wgs84.latitude_deg << ","
            << "\"lon\":" << scene.origin_wgs84.longitude_deg << ","
            << std::setprecision(2)
            << "\"alt\":" << scene.origin_wgs84.altitude_m << ","
            << "\"zone\":\"" << scene.origin_utm.zone_string() << "\","
            << "\"epsg\":" << scene.origin_utm.epsg_code();
    }
    out << "},\"meshes\":[";
    bool first_m = true;
    auto sep_m = [&]() {
        if (!first_m) out << ",";
        first_m = false;
    };

    if (!scene.ground_surface.empty()) {
        sep_m();
        emit_mesh_json(scene.ground_surface, "ground", "Superficie del terreno");
    }
    for (const auto& hz : scene.horizons) {
        if (hz.mesh.empty()) continue;
        sep_m();
        std::ostringstream inf;
        inf << std::fixed << std::setprecision(2) << "Prof. media: " << hz.mean_depth_m << " m";
        emit_mesh_json(hz.mesh, "horizon", inf.str());
    }
    for (const auto& c : scene.conduits) {
        if (c.mesh.empty()) continue;
        sep_m();
        std::ostringstream inf;
        inf << std::fixed << std::setprecision(2)
            << "Prof: " << c.mean_depth_m << " m | Diam: " << (c.estimated_radius_m * 200.0) << " cm";
        if (!c.centerline_wgs84.empty()) {
            inf << std::setprecision(6) << " | (" << c.centerline_wgs84.front().latitude_deg
                << ", " << c.centerline_wgs84.front().longitude_deg << ")";
        }
        emit_mesh_json(c.mesh, "conduit", inf.str());
    }
    for (const auto& a : scene.anomalies) {
        if (a.mesh.empty()) continue;
        sep_m();
        std::ostringstream inf;
        inf << std::fixed << std::setprecision(2) << "Prof: " << a.depth_m << " m";
        emit_mesh_json(a.mesh, "anomaly", inf.str());
    }
    out << "],\"pts\":[";
    const std::size_t pt_step = std::max<std::size_t>(1, scene.point_cloud.size() / 1500);
    bool first_p = true;
    out << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < scene.point_cloud.size(); i += pt_step) {
        if (!first_p) out << ",";
        first_p = false;
        const auto& p = scene.point_cloud.points[i];
        out << "[" << p.enu.east_m << "," << p.enu.north_m << "," << p.enu.up_m << "," << p.intensity << "]";
    }

    // B-scan subsampled traces + envelopes + coordinates
    out << "],\"bscan\":{\"depths\":[";
    for (std::size_t j = 0, k = 0; j < scene.fluctuations.depth_axis_m.size(); j += s_step, ++k) {
        if (k > 0) out << ",";
        out << scene.fluctuations.depth_axis_m[j];
    }
    out << "],\"traces\":[";
    for (std::size_t i = 0, ti = 0; i < n_tr; i += tr_step, ++ti) {
        if (ti > 0) out << ",";
        const auto& tr = bscan.traces[i];
        const EnuCoord e = tr.position_wgs84.has_value()
                               ? geo::wgs84_to_enu(*tr.position_wgs84, scene.origin_wgs84)
                               : EnuCoord{};
        out << "{\"x\":" << tr.distance_along_track_m
            << ",\"enu\":[" << e.east_m << "," << e.north_m << "," << e.up_m << "],\"s\":[";
        for (std::size_t j = 0, sj = 0; j < tr.samples.size(); j += s_step, ++sj) {
            if (sj > 0) out << ",";
            out << std::setprecision(1) << tr.samples[j];
        }
        out << "],\"e\":[";
        for (std::size_t j = 0, sj = 0; j < tr.envelope.size(); j += s_step, ++sj) {
            if (sj > 0) out << ",";
            out << std::setprecision(1) << tr.envelope[j];
        }
        out << "]}";
    }
    out << "],\"freq\":[";
    for (std::size_t k = 0; k < scene.fluctuations.freq_axis_mhz.size() &&
                            scene.fluctuations.freq_axis_mhz[k] <= 1600.0; ++k) {
        if (k > 0) out << ",";
        out << "[" << std::setprecision(1) << scene.fluctuations.freq_axis_mhz[k]
            << "," << std::setprecision(2) << scene.fluctuations.mean_psd_db[k] << "]";
    }
    out << "]}};\n";

    out << R"HTML(
if (SCENE.crs.zone) {
  document.getElementById('crsBanner').textContent =
    `WGS84 (${SCENE.crs.lat}, ${SCENE.crs.lon}, ${SCENE.crs.alt}m) | UTM ${SCENE.crs.zone} (EPSG:${SCENE.crs.epsg})`;
}
const objHtml = SCENE.meshes.filter(m => m.cat !== 'ground').map(m => {
  const c = m.rgba.map((v,i)=>i<3?Math.round(v*255):v);
  return `<div><span class="badge" style="background:rgb(${c[0]},${c[1]},${c[2]})"></span><b>${m.name}</b> — ${m.info}</div>`;
}).join('');
document.getElementById('objList').innerHTML = objHtml || '<div>Sin objetos discretos sobre el umbral</div>';

let selTrace = Math.floor(SCENE.bscan.traces.length / 2);
let maxAmp = 1;
SCENE.bscan.traces.forEach(t => t.s.forEach(v => { if (Math.abs(v) > maxAmp) maxAmp = Math.abs(v); }));

// Compute 3D bounding center
let minB = [1e9,1e9,1e9], maxB = [-1e9,-1e9,-1e9];
SCENE.meshes.forEach(m => m.v.forEach(p => {
  for(let k=0;k<3;k++){ minB[k]=Math.min(minB[k],p[k]); maxB[k]=Math.max(maxB[k],p[k]); }
}));
if (minB[0] > maxB[0]) { minB=[0,0,-2]; maxB=[5,5,0]; }
const center3d = [(minB[0]+maxB[0])/2, (minB[1]+maxB[1])/2, (minB[2]+maxB[2])/2];
const span3d = Math.max(2.0, maxB[0]-minB[0], maxB[1]-minB[1], maxB[2]-minB[2]);

let yaw = -0.65, pitch = 0.52, zoom = 1.25;
let dragging = false, lx = 0, ly = 0;
const cv3d = document.getElementById('cv3d');
cv3d.addEventListener('mousedown', e => { dragging = true; lx = e.clientX; ly = e.clientY; });
window.addEventListener('mouseup', () => dragging = false);
window.addEventListener('mousemove', e => {
  if (!dragging) return;
  yaw += (e.clientX - lx) * 0.008;
  pitch = Math.max(-1.45, Math.min(1.45, pitch + (e.clientY - ly) * 0.008));
  lx = e.clientX; ly = e.clientY;
  draw3D();
});
cv3d.addEventListener('wheel', e => {
  e.preventDefault();
  zoom = Math.max(0.35, Math.min(4.0, zoom * (e.deltaY > 0 ? 0.9 : 1.11)));
  draw3D();
}, {passive:false});
['chkConduits','chkHorizons','chkAnomalies','chkPoints','chkGround'].forEach(id =>
  document.getElementById(id).addEventListener('change', draw3D));

function project(p, W, H) {
  let x = p[0] - center3d[0], y = p[1] - center3d[1], z = (p[2] - center3d[2]) * 2.2;
  const cy = Math.cos(yaw), sy = Math.sin(yaw);
  let x1 = x * cy - y * sy, y1 = x * sy + y * cy;
  const cp = Math.cos(pitch), sp = Math.sin(pitch);
  let y2 = y1 * cp - z * sp, z2 = y1 * sp + z * cp;
  const dist = span3d * 1.9 / zoom;
  const d = dist + y2;
  if (d < 0.2) return null;
  const f = Math.min(W, H) * 0.85 / d;
  return [W * 0.5 + x1 * f, H * 0.52 - z2 * f, d];
}

function draw3D() {
  const r = cv3d.getBoundingClientRect();
  cv3d.width = r.width; cv3d.height = r.height;
  const ctx = cv3d.getContext('2d');
  const W = cv3d.width, H = cv3d.height;
  ctx.fillStyle = '#090d16'; ctx.fillRect(0, 0, W, H);

  const show = {
    conduit: document.getElementById('chkConduits').checked,
    horizon: document.getElementById('chkHorizons').checked,
    anomaly: document.getElementById('chkAnomalies').checked,
    ground: document.getElementById('chkGround').checked
  };

  const prims = [];
  SCENE.meshes.forEach(m => {
    if (!show[m.cat]) return;
    const pv = m.v.map(v => project(v, W, H));
    m.f.forEach(tri => {
      const a = pv[tri[0]], b = pv[tri[1]], c = pv[tri[2]];
      if (!a || !b || !c) return;
      const v0 = m.v[tri[0]], v1 = m.v[tri[1]], v2 = m.v[tri[2]];
      const ux=v1[0]-v0[0], uy=v1[1]-v0[1], uz=v1[2]-v0[2];
      const vx=v2[0]-v0[0], vy=v2[1]-v0[1], vz=v2[2]-v0[2];
      let nx = uy*vz - uz*vy, ny = uz*vx - ux*vz, nz = ux*vy - uy*vx;
      const nl = Math.hypot(nx,ny,nz) || 1; nx/=nl; ny/=nl; nz/=nl;
      const shade = 0.45 + 0.55 * Math.abs(0.35*nx - 0.45*ny + 0.82*nz);
      prims.push({
        d: (a[2] + b[2] + c[2]) / 3,
        pts: [a, b, c],
        col: `rgba(${Math.round(m.rgba[0]*255*shade)},${Math.round(m.rgba[1]*255*shade)},${Math.round(m.rgba[2]*255*shade)},${m.rgba[3]})`
      });
    });
  });

  prims.sort((p, q) => q.d - p.d);
  prims.forEach(p => {
    ctx.fillStyle = p.col;
    ctx.beginPath();
    ctx.moveTo(p.pts[0][0], p.pts[0][1]);
    ctx.lineTo(p.pts[1][0], p.pts[1][1]);
    ctx.lineTo(p.pts[2][0], p.pts[2][1]);
    ctx.closePath();
    ctx.fill();
  });

  if (document.getElementById('chkPoints').checked) {
    SCENE.pts.forEach(pt => {
      const pr = project(pt, W, H);
      if (!pr) return;
      const t = pt[3];
      ctx.fillStyle = `rgba(${Math.round(40+215*t)},${Math.round(180*(1-0.4*t))},${Math.round(255*(1-t))},0.75)`;
      ctx.fillRect(pr[0]-1.5, pr[1]-1.5, 3, 3);
    });
  }

  // Draw active virtual laser probe at selected trace
  if (SCENE.bscan.traces.length > 0) {
    const tr = SCENE.bscan.traces[selTrace];
    const maxD = SCENE.bscan.depths[SCENE.bscan.depths.length - 1] || 2.0;
    const pTop = project(tr.enu, W, H);
    const pBot = project([tr.enu[0], tr.enu[1], tr.enu[2] - maxD], W, H);
    if (pTop && pBot) {
      ctx.strokeStyle = '#38bdf8'; ctx.lineWidth = 2.2;
      ctx.beginPath(); ctx.moveTo(pTop[0], pTop[1]); ctx.lineTo(pBot[0], pBot[1]); ctx.stroke();
    }
  }
}

const cvB = document.getElementById('cvBscan');
function drawBscan() {
  const r = cvB.getBoundingClientRect();
  cvB.width = r.width; cvB.height = r.height;
  const ctx = cvB.getContext('2d');
  const W = cvB.width, H = cvB.height;
  const trs = SCENE.bscan.traces;
  if (!trs.length) return;
  const ns = trs[0].s.length;
  const img = ctx.createImageData(W, H);
  const clip = 0.6 * maxAmp;
  for (let py = 0; py < H; py++) {
    const j = Math.min(ns - 1, Math.floor((py / H) * ns));
    for (let px = 0; px < W; px++) {
      const i = Math.min(trs.length - 1, Math.floor((px / W) * trs.length));
      const v = Math.max(-1, Math.min(1, trs[i].s[j] / clip));
      const idx = (py * W + px) * 4;
      if (v < 0) {
        const t = -v;
        img.data[idx]   = Math.round(235*(1-t) + 15*t);
        img.data[idx+1] = Math.round(240*(1-t) + 90*t);
        img.data[idx+2] = Math.round(248*(1-t) + 235*t);
      } else {
        const t = v;
        img.data[idx]   = Math.round(235*(1-t) + 235*t);
        img.data[idx+1] = Math.round(240*(1-t) + 40*t);
        img.data[idx+2] = Math.round(248*(1-t) + 35*t);
      }
      img.data[idx+3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const sx = (selTrace / Math.max(1, trs.length - 1)) * W;
  ctx.strokeStyle = '#38bdf8'; ctx.lineWidth = 2;
  ctx.beginPath(); ctx.moveTo(sx, 0); ctx.lineTo(sx, H); ctx.stroke();
  document.getElementById('traceInfo').textContent = `Traza #${selTrace} (x = ${trs[selTrace].x.toFixed(2)} m)`;
}

cvB.addEventListener('mousedown', e => {
  const r = cvB.getBoundingClientRect();
  selTrace = Math.max(0, Math.min(SCENE.bscan.traces.length - 1,
    Math.round(((e.clientX - r.left) / r.width) * (SCENE.bscan.traces.length - 1))));
  drawBscan(); drawAscan(); draw3D();
});

const cvA = document.getElementById('cvAscan');
function drawAscan() {
  const r = cvA.getBoundingClientRect();
  cvA.width = r.width; cvA.height = r.height;
  const ctx = cvA.getContext('2d');
  const W = cvA.width, H = cvA.height;
  ctx.fillStyle = '#090d16'; ctx.fillRect(0, 0, W, H);
  const tr = SCENE.bscan.traces[selTrace];
  if (!tr) return;

  const topH = H * 0.56;
  const midY = topH * 0.5;
  let localMax = 1;
  tr.e.forEach(v => { if (v > localMax) localMax = v; });

  ctx.strokeStyle = '#1e293b';
  ctx.beginPath(); ctx.moveTo(0, midY); ctx.lineTo(W, midY); ctx.stroke();

  // Envelope +-
  ctx.strokeStyle = '#f59e0b'; ctx.lineWidth = 1.4;
  [1, -1].forEach(sg => {
    ctx.beginPath();
    tr.e.forEach((v, j) => {
      const x = (j / (tr.e.length - 1)) * W;
      const y = midY - sg * (v / localMax) * (topH * 0.44);
      if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    });
    ctx.stroke();
  });

  // Waveform s(t)
  ctx.strokeStyle = '#22d3ee'; ctx.lineWidth = 1.5;
  ctx.beginPath();
  tr.s.forEach((v, j) => {
    const x = (j / (tr.s.length - 1)) * W;
    const y = midY - (v / localMax) * (topH * 0.44);
    if (j === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();

  // Bottom strip: FFT Spectrum (dB)
  const specTop = topH + 14, specH = H - specTop - 8;
  ctx.strokeStyle = '#a855f7'; ctx.lineWidth = 1.7;
  ctx.beginPath();
  SCENE.bscan.freq.forEach((fp, k) => {
    const x = (fp[0] / 1600.0) * W;
    const db = Math.max(-50, Math.min(0, fp[1]));
    const y = specTop + (db / -50.0) * specH;
    if (k === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();
  ctx.fillStyle = '#94a3b8'; ctx.font = '11px monospace';
  ctx.fillText('Espectro FFT [0 - 1600 MHz] (0 a -50 dB)', 10, specTop + 14);
}

window.addEventListener('resize', () => { draw3D(); drawBscan(); drawAscan(); });
draw3D(); drawBscan(); drawAscan();
</script>
</body>
</html>
)HTML";
}

}  // namespace gpr3d::viz

