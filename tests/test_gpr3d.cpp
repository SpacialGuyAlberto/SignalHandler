#include "gpr3d/core/endian.hpp"
#include "gpr3d/core/types.hpp"
#include "gpr3d/dsp/fft.hpp"
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
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check_true(bool cond, const char* expr, int line) {
    if (!cond) {
        std::cerr << "[FAIL] Line " << line << ": expected true: " << expr << "\n";
        ++g_failures;
    }
}

void check_near(double actual, double expected, double tol, const char* label, int line) {
    if (std::abs(actual - expected) > tol || !std::isfinite(actual)) {
        std::cerr << "[FAIL] Line " << line << " (" << label << "): actual=" << actual
                  << ", expected=" << expected << " (tol=" << tol << ")\n";
        ++g_failures;
    }
}

#define CHECK(expr) check_true((expr), #expr, __LINE__)
#define CHECK_NEAR(a, b, tol) check_near((a), (b), (tol), #a, __LINE__)

void test_geodesy_transforms() {
    std::cout << "[TEST] Geodesy WGS84 <-> UTM / ECEF / ENU ...\n";

    // Berlin reference point (UTM Zone 33N)
    const gpr3d::Wgs84Coord berlin{.latitude_deg = 52.520008, .longitude_deg = 13.404954, .altitude_m = 34.5};
    const auto utm = gpr3d::geo::wgs84_to_utm(berlin);
    CHECK(utm.zone == 33);
    CHECK(utm.is_northern);
    CHECK(utm.epsg_code() == 32633);
    // Known WGS84/UTM 33N value for (52.520008, 13.404954): E ~ 391777.05 m, N ~ 5820073.84 m
    CHECK_NEAR(utm.easting_m, 391777.05, 1.0);
    CHECK_NEAR(utm.northing_m, 5820073.84, 1.0);

    const auto berlin_rt = gpr3d::geo::utm_to_wgs84(utm);
    CHECK_NEAR(berlin_rt.latitude_deg, berlin.latitude_deg, 1e-10);
    CHECK_NEAR(berlin_rt.longitude_deg, berlin.longitude_deg, 1e-10);
    CHECK_NEAR(berlin_rt.altitude_m, berlin.altitude_m, 1e-9);

    // Southern hemisphere round-trip (Santiago de Chile, Zone 19S)
    const gpr3d::Wgs84Coord santiago{.latitude_deg = -33.448890, .longitude_deg = -70.669265, .altitude_m = 570.0};
    const auto utm_s = gpr3d::geo::wgs84_to_utm(santiago);
    CHECK(utm_s.zone == 19);
    CHECK(!utm_s.is_northern);
    CHECK(utm_s.epsg_code() == 32719);
    const auto santiago_rt = gpr3d::geo::utm_to_wgs84(utm_s);
    CHECK_NEAR(santiago_rt.latitude_deg, santiago.latitude_deg, 1e-10);
    CHECK_NEAR(santiago_rt.longitude_deg, santiago.longitude_deg, 1e-10);

    // Special Norway 32V zone rule (Bergen: 60.3913 N, 5.3221 E -> Zone 32 instead of 31)
    CHECK(gpr3d::geo::compute_utm_zone(60.3913, 5.3221) == 32);

    // ECEF & ENU round-trips
    const auto ecef = gpr3d::geo::wgs84_to_ecef(berlin);
    const auto ecef_rt = gpr3d::geo::ecef_to_wgs84(ecef);
    CHECK_NEAR(ecef_rt.latitude_deg, berlin.latitude_deg, 1e-10);
    CHECK_NEAR(ecef_rt.longitude_deg, berlin.longitude_deg, 1e-10);
    CHECK_NEAR(ecef_rt.altitude_m, berlin.altitude_m, 1e-6);

    const gpr3d::EnuCoord target_enu{.east_m = 12.5, .north_m = -8.25, .up_m = -1.35};
    const auto shifted_wgs = gpr3d::geo::enu_to_wgs84(target_enu, berlin);
    const auto recovered_enu = gpr3d::geo::wgs84_to_enu(shifted_wgs, berlin);
    CHECK_NEAR(recovered_enu.east_m, target_enu.east_m, 1e-6);
    CHECK_NEAR(recovered_enu.north_m, target_enu.north_m, 1e-6);
    CHECK_NEAR(recovered_enu.up_m, target_enu.up_m, 1e-6);

    // Lever-arm offset along heading = 90 deg (due East): forward -> +East, right -> -North
    const auto ant_wgs = gpr3d::geo::apply_lever_arm(berlin, 90.0, 1.20, 0.40, -0.30);
    const auto ant_enu = gpr3d::geo::wgs84_to_enu(ant_wgs, berlin);
    CHECK_NEAR(ant_enu.east_m, 1.20, 1e-5);
    CHECK_NEAR(ant_enu.north_m, -0.40, 1e-5);
    CHECK_NEAR(ant_enu.up_m, -0.30, 1e-5);
}

void test_nmea_and_ibm_float() {
    std::cout << "[TEST] NMEA-0183 GGA & IBM 360 float decoding ...\n";

    // Classic NMEA-0183 GPGGA test vector with valid XOR checksum *47
    const std::string nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    const auto fix = gpr3d::io::parse_nmea_gga(nmea);
    CHECK(fix.has_value());
    if (fix.has_value()) {
        CHECK_NEAR(fix->wgs84.latitude_deg, 48.0 + 7.038 / 60.0, 1e-6);
        CHECK_NEAR(fix->wgs84.longitude_deg, 11.0 + 31.0 / 60.0, 1e-6);
        CHECK_NEAR(fix->wgs84.altitude_m, 545.4, 1e-4);
        CHECK(fix->fix_quality == 1);
    }

    // Corrupted checksum must be rejected
    const std::string bad_nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48";
    CHECK(!gpr3d::io::parse_nmea_gga(bad_nmea).has_value());

    // IBM 360 float: 0x41100000 = +16^(65-64) * (1/16) = +1.0; 0xC2760000 = -16^(66-64) * (118/256) = -118.0
    CHECK_NEAR(gpr3d::detail::ibm_to_double(0x00000000U), 0.0, 1e-12);
    CHECK_NEAR(gpr3d::detail::ibm_to_double(0x41100000U), 1.0, 1e-12);
    CHECK_NEAR(gpr3d::detail::ibm_to_double(0xC2760000U), -118.0, 1e-12);
}

void test_fft_and_hilbert() {
    std::cout << "[TEST] FFT & Hilbert Analytic Envelope ...\n";

    constexpr std::size_t N = 256;
    std::vector<double> sig(N);
    std::vector<std::complex<double>> csig(N);
    for (std::size_t i = 0; i < N; ++i) {
        const double phase = 2.0 * std::numbers::pi * 8.0 * static_cast<double>(i) / static_cast<double>(N);
        sig[i] = 2.5 * std::cos(phase);
        csig[i] = {sig[i], 0.0};
    }

    const auto spec = gpr3d::dsp::fft_forward(csig);
    const auto rec = gpr3d::dsp::fft_inverse(spec);
    for (std::size_t i = 0; i < N; ++i) {
        CHECK_NEAR(rec[i].real(), sig[i], 1e-10);
        CHECK_NEAR(rec[i].imag(), 0.0, 1e-10);
    }

    // Analytic envelope of pure periodic cosine 2.5 * cos(2*pi*8*n/N) is identically 2.5!
    const auto env = gpr3d::dsp::hilbert_envelope(sig);
    CHECK(env.size() == N);
    for (std::size_t i = 16; i + 16 < N; ++i) {
        CHECK_NEAR(env[i], 2.5, 1e-9);
    }
}

void test_radar_io_roundtrips() {
    std::cout << "[TEST] SEG-Y (.sgy), GSSI (.dzt), and MALA (.rd3/.rad/.cor) I/O round-trips ...\n";

    const auto tmp_dir = std::filesystem::temp_directory_path() / "gpr3d_unit_tests";
    std::filesystem::create_directories(tmp_dir);

    gpr3d::sim::SyntheticSurveyConfig cfg{};
    cfg.num_traces = 60;
    cfg.samples_per_trace = 256;
    cfg.noise_std = 0.0;
    const auto orig = gpr3d::sim::SyntheticGenerator::generate_bscan(cfg);

    // 1. SEG-Y round-trip
    const auto sgy_path = tmp_dir / "test_line.sgy";
    gpr3d::io::SegyReader::write(sgy_path, orig);
    const auto sgy_read = gpr3d::io::read_radar_file(sgy_path);
    CHECK(sgy_read.num_traces() == orig.num_traces());
    CHECK(sgy_read.num_samples() == orig.num_samples());
    CHECK_NEAR(sgy_read.metadata.sample_interval_ns, orig.metadata.sample_interval_ns, 1e-4);
    CHECK_NEAR(sgy_read.metadata.relative_permittivity, orig.metadata.relative_permittivity, 1e-4);
    CHECK(sgy_read.traces[25].position_wgs84.has_value());
    if (sgy_read.traces[25].position_wgs84.has_value()) {
        CHECK_NEAR(sgy_read.traces[25].position_wgs84->latitude_deg,
                   orig.traces[25].position_wgs84->latitude_deg, 1e-6);
        CHECK_NEAR(sgy_read.traces[25].position_wgs84->longitude_deg,
                   orig.traces[25].position_wgs84->longitude_deg, 1e-6);
        CHECK_NEAR(sgy_read.traces[25].position_wgs84->altitude_m,
                   orig.traces[25].position_wgs84->altitude_m, 2e-3);
    }
    CHECK_NEAR(sgy_read.traces[10].samples[100], orig.traces[10].samples[100], 1e-2);

    // 2. GSSI DZT round-trip
    const auto dzt_path = tmp_dir / "test_line.dzt";
    gpr3d::io::DztReader::write(dzt_path, orig);
    const auto dzt_read = gpr3d::io::read_radar_file(dzt_path);
    CHECK(dzt_read.num_traces() == orig.num_traces());
    CHECK(dzt_read.num_samples() == orig.num_samples());
    CHECK_NEAR(dzt_read.metadata.sample_interval_ns, orig.metadata.sample_interval_ns, 1e-5);
    CHECK_NEAR(dzt_read.metadata.relative_permittivity, orig.metadata.relative_permittivity, 1e-3);

    // 3. MALA RD3 + RAD + COR round-trip
    const auto rd3_path = tmp_dir / "test_line.rd3";
    gpr3d::io::Rd3Reader::write(rd3_path, orig);
    const auto rd3_read = gpr3d::io::read_radar_file(rd3_path);
    CHECK(rd3_read.num_traces() == orig.num_traces());
    CHECK(rd3_read.num_samples() == orig.num_samples());
    CHECK_NEAR(rd3_read.metadata.sample_interval_ns, orig.metadata.sample_interval_ns, 1e-5);
    CHECK(rd3_read.traces[40].position_wgs84.has_value());
    if (rd3_read.traces[40].position_wgs84.has_value()) {
        CHECK_NEAR(rd3_read.traces[40].position_wgs84->latitude_deg,
                   orig.traces[40].position_wgs84->latitude_deg, 1e-6);
        CHECK_NEAR(rd3_read.traces[40].position_wgs84->longitude_deg,
                   orig.traces[40].position_wgs84->longitude_deg, 1e-6);
    }

    std::filesystem::remove_all(tmp_dir);
}

void test_end_to_end_3d_conduit_localization() {
    std::cout << "[TEST] End-to-End DSP + Kirchhoff Migration + 3D Georeferenced Conduit Localization ...\n";

    // Simulate a straight due-East GPR line crossing a single fiber-optic conduit at x = 3.00 m, depth = 0.80 m
    gpr3d::sim::SyntheticSurveyConfig sim_cfg{};
    sim_cfg.num_traces = 151;
    sim_cfg.trace_spacing_m = 0.04;  // 0.00 m .. 6.00 m
    sim_cfg.samples_per_trace = 512;
    sim_cfg.sample_interval_ns = 0.06;
    sim_cfg.relative_permittivity = 9.0;
    sim_cfg.time_zero_ns = 3.0;
    sim_cfg.include_direct_wave = true;
    sim_cfg.include_wow_drift = true;
    sim_cfg.noise_std = 0.005;
    sim_cfg.azimuth_deg = 90.0;          // Due East
    sim_cfg.curvature_deg_per_m = 0.0;   // Perfectly straight
    sim_cfg.horizons.clear();
    sim_cfg.targets = {
        {.position_along_track_m = 3.00, .depth_m = 0.80, .radius_m = 0.04, .reflectivity = -1.0}
    };

    auto bscan = gpr3d::sim::SyntheticGenerator::generate_bscan(sim_cfg);
    // Slightly zero out surface elevation undulation for exact vertical geometry check
    for (std::size_t i = 0; i < bscan.num_traces(); ++i) {
        const gpr3d::EnuCoord enu{.east_m = static_cast<double>(i) * sim_cfg.trace_spacing_m,
                                  .north_m = 0.0,
                                  .up_m = 0.0};
        bscan.traces[i].position_wgs84 = gpr3d::geo::enu_to_wgs84(enu, sim_cfg.start_wgs84);
    }
    gpr3d::geo::update_headings_and_distances(bscan);

    gpr3d::dsp::ProcessingConfig proc_cfg{};
    proc_cfg.enable_dewow = true;
    proc_cfg.enable_time_zero = true;
    proc_cfg.fixed_time_zero_ns = sim_cfg.time_zero_ns;
    proc_cfg.enable_bandpass = true;
    proc_cfg.enable_background_removal = true;
    proc_cfg.enable_sec_gain = true;
    proc_cfg.enable_migration = true;
    proc_cfg.migration_aperture_m = 1.6;
    proc_cfg.compute_envelope = true;

    gpr3d::dsp::SignalProcessor::process(bscan, proc_cfg);

    gpr3d::dsp::InterpretationConfig interp_cfg{};
    interp_cfg.mode = gpr3d::dsp::ExtractionMode::ReflectorPeaksOnly;
    interp_cfg.min_intensity_threshold = 0.50;
    interp_cfg.min_depth_m = 0.20;
    interp_cfg.max_depth_m = 1.60;

    const auto cloud = gpr3d::dsp::RadarInterpreter::transform_to_3d(bscan, interp_cfg);
    CHECK(!cloud.empty());

    // Locate the peak 3D point (highest intensity = focused migrated conduit apex)
    auto best_it = std::max_element(
        cloud.points.begin(), cloud.points.end(),
        [](const gpr3d::Point3D& a, const gpr3d::Point3D& b) { return a.envelope < b.envelope; });

    CHECK(best_it != cloud.points.end());
    if (best_it != cloud.points.end()) {
        // Verify ENU horizontal & vertical coordinates match x=3.00 m East, North=0.00 m, Depth=0.80 m!
        CHECK_NEAR(best_it->enu.east_m, 3.00, 0.08);
        CHECK_NEAR(best_it->enu.north_m, 0.00, 0.02);
        CHECK_NEAR(best_it->depth_m, 0.80, 0.06);
        CHECK_NEAR(best_it->wgs84.altitude_m, sim_cfg.start_wgs84.altitude_m - best_it->depth_m, 1e-4);
    }

    // Verify 3D exporters (.ply ASCII, .ply binary, .csv, .xyz)
    const auto tmp_dir = std::filesystem::temp_directory_path() / "gpr3d_export_tests";
    std::filesystem::create_directories(tmp_dir);

    const auto ply_ascii = tmp_dir / "cloud_ascii.ply";
    const auto ply_bin = tmp_dir / "cloud_bin.ply";
    const auto csv_file = tmp_dir / "cloud.csv";
    const auto xyz_file = tmp_dir / "cloud.xyz";

    gpr3d::io::Exporter3D::write_ply(ply_ascii, cloud, {.binary = false});
    gpr3d::io::Exporter3D::write_ply(ply_bin, cloud, {.binary = true});
    gpr3d::io::Exporter3D::write_csv(csv_file, cloud);
    gpr3d::io::Exporter3D::write_xyz(xyz_file, cloud);

    CHECK(std::filesystem::exists(ply_ascii) && std::filesystem::file_size(ply_ascii) > 200);
    CHECK(std::filesystem::exists(ply_bin) && std::filesystem::file_size(ply_bin) > 200);
    CHECK(std::filesystem::exists(csv_file) && std::filesystem::file_size(csv_file) > 100);
    CHECK(std::filesystem::exists(xyz_file) && std::filesystem::file_size(xyz_file) > 100);

    std::filesystem::remove_all(tmp_dir);
}

void test_3d_objects_and_graphical_fluctuations() {
    std::cout << "[TEST] 3D Semantic Object Reconstruction (.obj/.glb/.geojson) & Graphical Fluctuations (.png/.svg/.html) ...\n";

    gpr3d::sim::SyntheticSurveyConfig sim_cfg{};
    sim_cfg.num_traces = 180;
    sim_cfg.samples_per_trace = 512;
    sim_cfg.sample_interval_ns = 0.08;
    sim_cfg.trace_spacing_m = 0.04;
    sim_cfg.noise_std = 0.008;

    const auto grid = gpr3d::sim::SyntheticGenerator::generate_3d_grid(3, 0.50, sim_cfg);
    CHECK(grid.size() == 3);

    gpr3d::dsp::ObjectTransformConfig tf_cfg{};
    tf_cfg.conduit_threshold = 0.40;
    tf_cfg.horizon_threshold = 0.18;

    const auto scene = gpr3d::dsp::SignalObjectTransformer::transform_grid(grid, tf_cfg);
    CHECK(scene.has_valid_crs);
    CHECK(!scene.ground_surface.empty());
    CHECK(!scene.conduits.empty());
    CHECK(!scene.horizons.empty());

    // Verify reconstructed 3D conduit meshes and georeferenced centerlines
    for (const auto& cond : scene.conduits) {
        CHECK(!cond.mesh.empty());
        CHECK(cond.centerline_wgs84.size() >= 2);
        CHECK(cond.centerline_utm.size() == cond.centerline_wgs84.size());
        CHECK(cond.mean_depth_m > 0.20 && cond.mean_depth_m < 1.80);
    }

    // Verify signal fluctuation analytics
    CHECK(scene.fluctuations.per_trace.size() == grid.front().num_traces());
    CHECK(scene.fluctuations.rep_waveform.size() == grid.front().num_samples());
    CHECK(scene.fluctuations.rep_envelope.size() == grid.front().num_samples());
    CHECK(!scene.fluctuations.freq_axis_mhz.empty());
    CHECK(scene.fluctuations.mean_psd_db.size() == scene.fluctuations.freq_axis_mhz.size());

    const auto tmp_dir = std::filesystem::temp_directory_path() / "gpr3d_scene_viz_tests";
    std::filesystem::create_directories(tmp_dir);

    // 1. Wavefront OBJ + MTL
    const auto obj_path = tmp_dir / "scene.obj";
    const auto mtl_path = tmp_dir / "scene.mtl";
    gpr3d::io::SceneExporter3D::write_obj(obj_path, scene);
    CHECK(std::filesystem::exists(obj_path) && std::filesystem::file_size(obj_path) > 1000);
    CHECK(std::filesystem::exists(mtl_path) && std::filesystem::file_size(mtl_path) > 100);

    // 2. Khronos glTF 2.0 Binary (.glb) header & exact chunk length verification
    const auto glb_path = tmp_dir / "scene.glb";
    gpr3d::io::SceneExporter3D::write_glb(glb_path, scene);
    CHECK(std::filesystem::exists(glb_path));
    {
        const auto fsize = std::filesystem::file_size(glb_path);
        CHECK(fsize > 500);
        std::ifstream glb_in(glb_path, std::ios::binary);
        std::uint8_t hdr[12]{};
        glb_in.read(reinterpret_cast<char*>(hdr), 12);
        CHECK(gpr3d::detail::read_u32_le(hdr + 0) == 0x46546C67U);  // "glTF"
        CHECK(gpr3d::detail::read_u32_le(hdr + 4) == 2U);           // version 2
        CHECK(gpr3d::detail::read_u32_le(hdr + 8) == static_cast<std::uint32_t>(fsize));
    }

    // 3. 3D GeoJSON
    const auto geojson_path = tmp_dir / "scene.geojson";
    gpr3d::io::SceneExporter3D::write_geojson(geojson_path, scene);
    CHECK(std::filesystem::exists(geojson_path) && std::filesystem::file_size(geojson_path) > 300);

    // 4. Interactive HTML5 3D + Signal Fluctuation Dashboard
    const auto html_path = tmp_dir / "dashboard.html";
    gpr3d::viz::FluctuationPlotter::write_html_dashboard(html_path, scene);
    CHECK(std::filesystem::exists(html_path) && std::filesystem::file_size(html_path) > 4000);

#ifdef GPR3D_USE_CAIRO
    // 5. Cairo PNG & Vector SVG 4-panel Fluctuation Charts
    const auto png_path = tmp_dir / "fluctuations.png";
    const auto svg_path = tmp_dir / "fluctuations.svg";
    gpr3d::viz::FluctuationPlotter::write_png(png_path, scene);
    gpr3d::viz::FluctuationPlotter::write_svg(svg_path, scene);
    CHECK(std::filesystem::exists(png_path) && std::filesystem::file_size(png_path) > 5000);
    CHECK(std::filesystem::exists(svg_path) && std::filesystem::file_size(svg_path) > 5000);

    // Verify PNG magic signature 89 50 4E 47
    std::ifstream png_in(png_path, std::ios::binary);
    unsigned char sig[4]{};
    png_in.read(reinterpret_cast<char*>(sig), 4);
    CHECK(sig[0] == 0x89 && sig[1] == 'P' && sig[2] == 'N' && sig[3] == 'G');
#endif

    std::filesystem::remove_all(tmp_dir);
}

}  // namespace

int main() {
    test_geodesy_transforms();
    test_nmea_and_ibm_float();
    test_fft_and_hilbert();
    test_radar_io_roundtrips();
    test_end_to_end_3d_conduit_localization();
    test_3d_objects_and_graphical_fluctuations();

    if (g_failures == 0) {
        std::cout << "\n[SUCCESS] All gpr3d tests passed!\n";
        return 0;
    }
    std::cerr << "\n[FAILURE] " << g_failures << " test assertion(s) failed.\n";
    return 1;
}

