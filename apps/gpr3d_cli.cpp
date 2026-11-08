#include "gpr3d/core/types.hpp"
#include "gpr3d/dsp/interpreter.hpp"
#include "gpr3d/dsp/object_transformer.hpp"
#include "gpr3d/dsp/signal_processor.hpp"
#include "gpr3d/geo/crs_transform.hpp"
#include "gpr3d/geo/trajectory_sync.hpp"
#include "gpr3d/io/dzt_reader.hpp"
#include "gpr3d/io/exporter_3d.hpp"
#include "gpr3d/io/gps_reader.hpp"
#include "gpr3d/io/rd3_reader.hpp"
#include "gpr3d/io/reader.hpp"
#include "gpr3d/io/scene_exporter.hpp"
#include "gpr3d/io/segy_reader.hpp"
#include "gpr3d/sim/synthetic_generator.hpp"
#include "gpr3d/viz/fluctuation_plotter.hpp"
#include "gpr3d/viz/gl_viewer.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void print_usage() {
    std::cout << R"(gpr3d - GPR/GDR Signal Processor, 3D Semantic Object Transformer & Fluctuation Visualizer

Usage:
  gpr3d simulate  [options]
  gpr3d inspect   <radar_file> [--gps <gps_file>]
  gpr3d process   -i <radar_file> [-i <line2> ...] -o <out.ply> [-o <out.csv>] [options]
  gpr3d transform -i <radar_file> [-i <line2> ...] -o <scene.glb> [-o <plot.png>] [-o <dash.html>] [options]
  gpr3d view      -i <radar_file> [-i <line2> ...] [options]

Subcommands:
  simulate    Generate realistic synthetic GPR B-scans (.sgy, .dzt, .rd3) with buried conduits & GPS
  inspect     Display header metadata, dielectric depth window, and WGS84/UTM trajectory bounds
  process     Filter/migrate GPR traces and export georeferenced 3D point clouds (.ply, .csv, .xyz)
  transform   Interpret GPR signals into 3D semantic objects (.obj, .glb, .geojson) & graphical
              signal fluctuations (.png, .svg, interactive 3D/2D .html, plus .ply/.csv)
  view        Launch native C++ OpenGL/GLFW interactive 3D object & live waveform fluctuation viewer

Options for 'transform' and 'view':
  -i, --input <path>        Input radar file (.sgy/.segy, .dzt, .rd3/.rad); repeatable for 3D grids
  -g, --gps <path>          External GNSS track file (.csv, .cor, .nmea)
  -o, --output <path>       Repeatable output targets (auto-detected by extension):
                              3D Meshes / GIS : .obj (+.mtl), .glb (glTF 2.0), .geojson
                              Fluctuation Viz : .png, .svg (Cairo 4-panel), .html (Interactive 3D+2D)
                              Point Clouds    : .ply, .csv, .xyz
  --epsr <val>              Override medium relative permittivity eps_r
  --conduit-thresh <0..1>   Normalized migrated envelope threshold for conduits [default: 0.42]
  --horizon-thresh <0..1>   Relative threshold for continuous stratigraphic layers [default: 0.20]
  --anomaly-thresh <0..1>   Threshold for localized 3D anomaly volumes [default: 0.32]
  --aperture <m>            2D Kirchhoff migration aperture in meters [default: 1.5]
  --frame <enu|utm>         Coordinate frame for .obj/.ply/.xyz [default: enu for .obj, utm for .ply]
  --origin <lat,lon,alt>    Fallback WGS84 origin when radar file has no GPS

Options for 'simulate':
  -o, --output <path>       Output radar file (.sgy, .dzt, or .rd3) [default: synthetic_survey.sgy]
  --gps-out <path>          Optional companion CSV GPS file to write
  --traces <N>              Traces per B-scan [default: 250]
  --samples <N>             Samples per trace [default: 512]
  --epsr <val>              Relative dielectric permittivity eps_r [default: 9.0]
  --freq <MHz>              Antenna center frequency in MHz [default: 400.0]
  --lat <deg> --lon <deg>   Starting WGS84 latitude and longitude [default: 52.520008, 13.404954]
  --alt <m>                 Starting surface elevation in meters [default: 34.5]
  --lines <N>               If > 1, generates N parallel 3D survey lines (<stem>_lineXX.<ext>)
  --line-spacing <m>        Cross-line spacing in meters when --lines > 1 [default: 0.50]

Options for 'process':
  -i, --input <path>        Input radar file; repeatable for multi-line 3D grids
  -g, --gps <path>          External GNSS track file (.csv, .cor, .nmea)
  -o, --output <path>       Output 3D point cloud (.ply, .csv, .xyz)
  --mode <volume|peaks|curtain>  Extraction mode [default: volume]
  --threshold <0..1>        Normalized Hilbert envelope cutoff [default: 0.18]
  --min-depth <m>           Minimum depth below surface in meters [default: 0.05]
  --max-depth <m>           Maximum depth below surface in meters [default: 10.0]
  --migrate                 Apply 2D Kirchhoff diffraction migration
  --binary-ply              Export PLY in compact binary_little_endian format
)";
}

std::string lower_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

void write_bscan_by_ext(const std::filesystem::path& path, const gpr3d::BScan& bscan) {
    const std::string ext = lower_str(path.extension().string());
    if (ext == ".sgy" || ext == ".segy") {
        gpr3d::io::SegyReader::write(path, bscan);
    } else if (ext == ".dzt") {
        gpr3d::io::DztReader::write(path, bscan);
    } else if (ext == ".rd3") {
        gpr3d::io::Rd3Reader::write(path, bscan);
    } else {
        throw std::runtime_error("Unsupported output radar extension '" + ext + "' (use .sgy, .dzt, or .rd3)");
    }
}

void write_gps_csv(const std::filesystem::path& path, const gpr3d::BScan& bscan) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open GPS CSV for writing: " + path.string());
    }
    out << "trace_id,timestamp_sec,latitude_deg,longitude_deg,altitude_m\n" << std::fixed;
    for (const auto& tr : bscan.traces) {
        if (!tr.position_wgs84.has_value()) {
            continue;
        }
        out << tr.trace_id << ','
            << std::setprecision(4) << tr.timestamp_sec.value_or(0.0) << ','
            << std::setprecision(8) << tr.position_wgs84->latitude_deg << ','
            << tr.position_wgs84->longitude_deg << ','
            << std::setprecision(3) << tr.position_wgs84->altitude_m << '\n';
    }
}

gpr3d::Wgs84Coord parse_origin_triplet(const std::string& s) {
    std::stringstream ss(s);
    std::string a, b, c;
    if (!std::getline(ss, a, ',') || !std::getline(ss, b, ',')) {
        throw std::invalid_argument("Expected --origin <lat,lon[,alt]>");
    }
    double alt = 0.0;
    if (std::getline(ss, c, ',') && !c.empty()) {
        alt = std::stod(c);
    }
    return gpr3d::Wgs84Coord{
        .latitude_deg = std::stod(a),
        .longitude_deg = std::stod(b),
        .altitude_m = alt
    };
}

int cmd_simulate(int argc, char** argv) {
    std::filesystem::path out_path = "synthetic_survey.sgy";
    std::filesystem::path gps_out_path;
    gpr3d::sim::SyntheticSurveyConfig cfg{};
    std::size_t num_lines = 1;
    double line_spacing_m = 0.50;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next_val = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument("Missing value after " + arg);
            return argv[++i];
        };

        if (arg == "-o" || arg == "--output") {
            out_path = next_val();
        } else if (arg == "--gps-out") {
            gps_out_path = next_val();
        } else if (arg == "--traces") {
            cfg.num_traces = static_cast<std::size_t>(std::stoull(next_val()));
        } else if (arg == "--samples") {
            cfg.samples_per_trace = static_cast<std::size_t>(std::stoull(next_val()));
        } else if (arg == "--epsr") {
            cfg.relative_permittivity = std::stod(next_val());
        } else if (arg == "--freq") {
            cfg.antenna_freq_mhz = std::stod(next_val());
        } else if (arg == "--lat") {
            cfg.start_wgs84.latitude_deg = std::stod(next_val());
        } else if (arg == "--lon") {
            cfg.start_wgs84.longitude_deg = std::stod(next_val());
        } else if (arg == "--alt") {
            cfg.start_wgs84.altitude_m = std::stod(next_val());
        } else if (arg == "--lines") {
            num_lines = static_cast<std::size_t>(std::stoull(next_val()));
        } else if (arg == "--line-spacing") {
            line_spacing_m = std::stod(next_val());
        } else {
            throw std::invalid_argument("Unknown simulate option: " + arg);
        }
    }

    if (out_path.has_parent_path()) {
        std::filesystem::create_directories(out_path.parent_path());
    }

    if (num_lines <= 1) {
        const auto bscan = gpr3d::sim::SyntheticGenerator::generate_bscan(cfg);
        write_bscan_by_ext(out_path, bscan);
        if (!gps_out_path.empty()) {
            write_gps_csv(gps_out_path, bscan);
        }
        std::cout << "[simulate] Generated " << bscan.num_traces() << " traces x "
                  << bscan.num_samples() << " samples -> " << out_path.string() << "\n";
    } else {
        const auto grid = gpr3d::sim::SyntheticGenerator::generate_3d_grid(num_lines, line_spacing_m, cfg);
        const auto dir = out_path.parent_path();
        const std::string stem = out_path.stem().string();
        const std::string ext = out_path.extension().string();

        for (std::size_t k = 0; k < grid.size(); ++k) {
            std::ostringstream fname;
            fname << stem << "_line" << std::setw(2) << std::setfill('0') << (k + 1) << ext;
            const auto lpath = dir.empty() ? std::filesystem::path(fname.str()) : (dir / fname.str());
            write_bscan_by_ext(lpath, grid[k]);
            if (lower_str(ext) == ".dzt") {
                auto cpath = lpath;
                cpath.replace_extension(".csv");
                write_gps_csv(cpath, grid[k]);
            }
            std::cout << "[simulate] Line " << (k + 1) << "/" << grid.size()
                      << " -> " << lpath.string() << "\n";
        }
    }
    return 0;
}

int cmd_inspect(int argc, char** argv) {
    if (argc < 3) {
        throw std::invalid_argument("inspect requires a radar file path");
    }
    const std::filesystem::path radar_path = argv[2];
    std::filesystem::path gps_path;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "-g" || arg == "--gps") && i + 1 < argc) {
            gps_path = argv[++i];
        }
    }

    auto bscan = gpr3d::io::read_radar_file(radar_path);
    if (!gps_path.empty()) {
        const auto fixes = gpr3d::io::read_gps_file(gps_path);
        if (!fixes.empty()) {
            gpr3d::geo::synchronize_trajectory(bscan, fixes);
        }
    }

    const double eps_r = std::max(1.0, bscan.metadata.relative_permittivity);
    const double v_m_ns = gpr3d::kSpeedOfLightMPerNs / std::sqrt(eps_r);
    const double max_depth_m = 0.5 * v_m_ns *
                               std::max(0.0, bscan.metadata.time_window_ns - bscan.metadata.time_zero_ns);

    double min_val = 0.0, max_val = 0.0, sum_sq = 0.0;
    std::size_t total_samples = 0, georef_count = 0;

    for (const auto& tr : bscan.traces) {
        if (tr.position_wgs84.has_value()) ++georef_count;
        for (double v : tr.samples) {
            if (total_samples == 0 || v < min_val) min_val = v;
            if (total_samples == 0 || v > max_val) max_val = v;
            sum_sq += v * v;
            ++total_samples;
        }
    }
    const double rms = (total_samples > 0) ? std::sqrt(sum_sq / static_cast<double>(total_samples)) : 0.0;

    std::cout << "=== GPR Radargram Inspection ===\n"
              << "File               : " << radar_path.string() << "\n"
              << "Format             : " << bscan.metadata.source_format << "\n"
              << "Traces (A-scans)   : " << bscan.num_traces() << "\n"
              << "Samples / Trace    : " << bscan.num_samples() << "\n"
              << std::fixed << std::setprecision(4)
              << "Sample Interval    : " << bscan.metadata.sample_interval_ns << " ns\n"
              << "Time Window        : " << bscan.metadata.time_window_ns << " ns (t0 = "
              << bscan.metadata.time_zero_ns << " ns)\n"
              << std::setprecision(1)
              << "Antenna Frequency  : " << bscan.metadata.antenna_freq_mhz << " MHz\n"
              << std::setprecision(3)
              << "Permittivity eps_r : " << eps_r << " (v = " << v_m_ns << " m/ns)\n"
              << "Max Depth Window   : " << max_depth_m << " m\n"
              << "Amplitude Range    : [" << min_val << ", " << max_val << "] (RMS = " << rms << ")\n"
              << "Georeferenced      : " << georef_count << " / " << bscan.num_traces() << " traces\n";

    if (georef_count > 0 && bscan.traces.front().position_wgs84.has_value() &&
        bscan.traces.back().position_wgs84.has_value()) {
        const auto w0 = *bscan.traces.front().position_wgs84;
        const auto w1 = *bscan.traces.back().position_wgs84;
        const auto u0 = gpr3d::geo::wgs84_to_utm(w0);
        const auto u1 = gpr3d::geo::wgs84_to_utm(w1, u0.zone);

        std::cout << std::setprecision(8)
                  << "Start WGS84        : (" << w0.latitude_deg << ", " << w0.longitude_deg
                  << std::setprecision(3) << ", " << w0.altitude_m << " m)\n"
                  << std::setprecision(8)
                  << "End WGS84          : (" << w1.latitude_deg << ", " << w1.longitude_deg
                  << std::setprecision(3) << ", " << w1.altitude_m << " m)\n"
                  << "UTM Zone / EPSG    : " << u0.zone_string() << " (EPSG:" << u0.epsg_code() << ")\n"
                  << "Start UTM (E, N)   : (" << u0.easting_m << ", " << u0.northing_m << " m)\n"
                  << "End UTM   (E, N)   : (" << u1.easting_m << ", " << u1.northing_m << " m)\n"
                  << "Track Length       : " << bscan.traces.back().distance_along_track_m << " m\n";
    }
    return 0;
}

int cmd_process(int argc, char** argv) {
    std::vector<std::filesystem::path> input_paths;
    std::filesystem::path gps_path;
    std::vector<std::filesystem::path> output_paths;

    gpr3d::io::ReadOptions read_opts{};
    gpr3d::dsp::ProcessingConfig proc_cfg{};
    gpr3d::dsp::InterpretationConfig interp_cfg{};
    gpr3d::io::PlyExportOptions ply_opts{};

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next_val = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument("Missing value after " + arg);
            return argv[++i];
        };

        if (arg == "-i" || arg == "--input") {
            input_paths.emplace_back(next_val());
        } else if (arg == "-g" || arg == "--gps") {
            gps_path = next_val();
        } else if (arg == "-o" || arg == "--output") {
            output_paths.emplace_back(next_val());
        } else if (arg == "--epsr") {
            const double e = std::stod(next_val());
            read_opts.override_epsr = e;
            interp_cfg.override_epsr = e;
        } else if (arg == "--mode") {
            const std::string m = lower_str(next_val());
            if (m == "volume") interp_cfg.mode = gpr3d::dsp::ExtractionMode::ThresholdedVolume;
            else if (m == "peaks") interp_cfg.mode = gpr3d::dsp::ExtractionMode::ReflectorPeaksOnly;
            else if (m == "curtain") interp_cfg.mode = gpr3d::dsp::ExtractionMode::FullCurtain;
            else throw std::invalid_argument("Invalid --mode '" + m + "'");
        } else if (arg == "--threshold") {
            interp_cfg.min_intensity_threshold = std::stod(next_val());
        } else if (arg == "--min-depth") {
            interp_cfg.min_depth_m = std::stod(next_val());
        } else if (arg == "--max-depth") {
            interp_cfg.max_depth_m = std::stod(next_val());
        } else if (arg == "--migrate") {
            proc_cfg.enable_migration = true;
        } else if (arg == "--aperture") {
            proc_cfg.migration_aperture_m = std::stod(next_val());
        } else if (arg == "--no-bg") {
            proc_cfg.enable_background_removal = false;
        } else if (arg == "--frame") {
            const std::string f = lower_str(next_val());
            ply_opts.primary_frame = (f == "enu") ? gpr3d::io::CoordFrame3D::LocalENU
                                                  : gpr3d::io::CoordFrame3D::UTM;
        } else if (arg == "--binary-ply") {
            ply_opts.binary = true;
        } else if (arg == "--utm-zone") {
            const int z = std::stoi(next_val());
            read_opts.utm_zone = z;
            interp_cfg.forced_utm_zone = z;
        } else if (arg == "--origin") {
            interp_cfg.fallback_origin_wgs84 = parse_origin_triplet(next_val());
        } else if (arg == "--azimuth") {
            interp_cfg.fallback_azimuth_deg = std::stod(next_val());
        } else {
            throw std::invalid_argument("Unknown process option: " + arg);
        }
    }

    if (input_paths.empty()) throw std::invalid_argument("process requires -i <radar_file>");
    if (output_paths.empty()) throw std::invalid_argument("process requires -o <output_3d_file>");

    std::vector<gpr3d::GpsFix> external_fixes;
    if (!gps_path.empty()) external_fixes = gpr3d::io::read_gps_file(gps_path);

    gpr3d::PointCloud3D cloud{};
    for (const auto& in_path : input_paths) {
        auto bscan = gpr3d::io::read_radar_file(in_path, read_opts);
        if (!external_fixes.empty()) gpr3d::geo::synchronize_trajectory(bscan, external_fixes);
        gpr3d::dsp::SignalProcessor::process(bscan, proc_cfg);
        gpr3d::dsp::RadarInterpreter::append_bscan_to_3d(cloud, bscan, interp_cfg);
    }

    for (const auto& out_path : output_paths) {
        if (out_path.has_parent_path()) std::filesystem::create_directories(out_path.parent_path());
        gpr3d::io::Exporter3D::write_auto(out_path, cloud, ply_opts);
        std::cout << "[process] Exported " << cloud.size()
                  << " georeferenced 3D points -> " << out_path.string() << "\n";
    }
    return 0;
}

int cmd_transform_or_view(int argc, char** argv, bool launch_viewer) {
    std::vector<std::filesystem::path> input_paths;
    std::filesystem::path gps_path;
    std::vector<std::filesystem::path> output_paths;

    gpr3d::io::ReadOptions read_opts{};
    gpr3d::dsp::ObjectTransformConfig tf_cfg{};
    gpr3d::io::SceneExportOptions scene_opts{};
    gpr3d::io::PlyExportOptions ply_opts{};

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next_val = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument("Missing value after " + arg);
            return argv[++i];
        };

        if (arg == "-i" || arg == "--input") {
            input_paths.emplace_back(next_val());
        } else if (arg == "-g" || arg == "--gps") {
            gps_path = next_val();
        } else if (arg == "-o" || arg == "--output") {
            output_paths.emplace_back(next_val());
        } else if (arg == "--epsr") {
            read_opts.override_epsr = std::stod(next_val());
        } else if (arg == "--conduit-thresh") {
            tf_cfg.conduit_threshold = std::stod(next_val());
        } else if (arg == "--horizon-thresh") {
            tf_cfg.horizon_threshold = std::stod(next_val());
        } else if (arg == "--anomaly-thresh") {
            tf_cfg.anomaly_threshold = std::stod(next_val());
        } else if (arg == "--aperture") {
            tf_cfg.migration_aperture_m = std::stod(next_val());
        } else if (arg == "--min-depth") {
            tf_cfg.min_depth_m = std::stod(next_val());
        } else if (arg == "--max-depth") {
            tf_cfg.max_depth_m = std::stod(next_val());
        } else if (arg == "--frame") {
            const std::string f = lower_str(next_val());
            const auto frame = (f == "utm") ? gpr3d::io::CoordFrame3D::UTM
                                            : gpr3d::io::CoordFrame3D::LocalENU;
            scene_opts.obj_coord_frame = frame;
            ply_opts.primary_frame = frame;
        } else if (arg == "--origin") {
            tf_cfg.fallback_origin_wgs84 = parse_origin_triplet(next_val());
        } else if (arg == "--azimuth") {
            tf_cfg.fallback_azimuth_deg = std::stod(next_val());
        } else if (!arg.empty() && arg[0] != '-') {
            // Allow positional radar input file convenience (`gpr3d view line.sgy`)
            input_paths.emplace_back(arg);
        } else {
            throw std::invalid_argument("Unknown option: " + arg);
        }
    }

    if (input_paths.empty()) {
        throw std::invalid_argument("Requires at least one radar file (-i <radar_file>)");
    }
    if (!launch_viewer && output_paths.empty()) {
        throw std::invalid_argument("transform requires at least one -o <output_file>");
    }

    std::vector<gpr3d::GpsFix> external_fixes;
    if (!gps_path.empty()) {
        external_fixes = gpr3d::io::read_gps_file(gps_path);
    }

    std::vector<gpr3d::BScan> bscans;
    bscans.reserve(input_paths.size());
    for (const auto& in_path : input_paths) {
        auto bscan = gpr3d::io::read_radar_file(in_path, read_opts);
        if (!external_fixes.empty()) {
            gpr3d::geo::synchronize_trajectory(bscan, external_fixes);
        }
        bscans.push_back(std::move(bscan));
    }

    const auto scene = gpr3d::dsp::SignalObjectTransformer::transform_grid(bscans, tf_cfg);

    std::cout << "[transform] Interpreted 3D Scene Summary:\n"
              << "  - Cylindrical Conduits / Pipes : " << scene.conduits.size() << "\n"
              << "  - Stratigraphic Horizons       : " << scene.horizons.size() << "\n"
              << "  - Localized 3D Anomalies       : " << scene.anomalies.size() << "\n"
              << "  - 3D Point Cloud Reflections   : " << scene.point_cloud.size() << "\n";

    for (const auto& out_path : output_paths) {
        if (out_path.has_parent_path()) {
            std::filesystem::create_directories(out_path.parent_path());
        }
        const std::string ext = lower_str(out_path.extension().string());
        if (ext == ".obj" || ext == ".glb" || ext == ".geojson" || ext == ".json") {
            gpr3d::io::SceneExporter3D::write_auto(out_path, scene, scene_opts);
        } else if (ext == ".png") {
            gpr3d::viz::FluctuationPlotter::write_png(out_path, scene);
        } else if (ext == ".svg") {
            gpr3d::viz::FluctuationPlotter::write_svg(out_path, scene);
        } else if (ext == ".html" || ext == ".htm") {
            gpr3d::viz::FluctuationPlotter::write_html_dashboard(out_path, scene);
        } else if (ext == ".ply" || ext == ".csv" || ext == ".xyz") {
            gpr3d::io::Exporter3D::write_auto(out_path, scene.point_cloud, ply_opts);
        } else {
            throw std::runtime_error(
                "Unsupported output extension '" + ext +
                "' (use .obj, .glb, .geojson, .png, .svg, .html, .ply, .csv, or .xyz)");
        }
        std::cout << "[transform] Wrote -> " << out_path.string() << "\n";
    }

    if (launch_viewer) {
        gpr3d::viz::GlSceneViewer::run(scene);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage();
            return 1;
        }
        const std::string subcmd = argv[1];
        if (subcmd == "-h" || subcmd == "--help" || subcmd == "help") {
            print_usage();
            return 0;
        }
        if (subcmd == "simulate") {
            return cmd_simulate(argc, argv);
        }
        if (subcmd == "inspect") {
            return cmd_inspect(argc, argv);
        }
        if (subcmd == "process") {
            return cmd_process(argc, argv);
        }
        if (subcmd == "transform") {
            return cmd_transform_or_view(argc, argv, false);
        }
        if (subcmd == "view") {
            return cmd_transform_or_view(argc, argv, true);
        }
        std::cerr << "Unknown subcommand '" << subcmd << "'\n\n";
        print_usage();
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "[gpr3d error] " << ex.what() << "\n";
        return 1;
    }
}
