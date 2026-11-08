#include "gpr3d/io/rd3_reader.hpp"
#include "gpr3d/core/endian.hpp"
#include "gpr3d/geo/trajectory_sync.hpp"
#include "gpr3d/io/gps_reader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace gpr3d::io {

namespace {

std::string trim_str(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    std::size_t idx = 0;
    while (idx < s.size() && std::isspace(static_cast<unsigned char>(s[idx]))) {
        ++idx;
    }
    return s.substr(idx);
}

std::string upper_str(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::filesystem::path find_companion_ext(
    const std::filesystem::path& base_path,
    const std::string& lower_ext,
    const std::string& upper_ext) {
    auto p1 = base_path;
    p1.replace_extension(lower_ext);
    if (std::filesystem::exists(p1)) {
        return p1;
    }
    auto p2 = base_path;
    p2.replace_extension(upper_ext);
    if (std::filesystem::exists(p2)) {
        return p2;
    }
    return {};
}

std::unordered_map<std::string, std::string> parse_rad_header(const std::filesystem::path& rad_path) {
    std::ifstream in(rad_path);
    if (!in) {
        throw std::runtime_error("Cannot open MALA .rad header file: " + rad_path.string());
    }
    std::unordered_map<std::string, std::string> kv;
    std::string line;
    while (std::getline(in, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        const std::string key = upper_str(trim_str(line.substr(0, colon)));
        const std::string val = trim_str(line.substr(colon + 1));
        if (!key.empty()) {
            kv[key] = val;
        }
    }
    return kv;
}

}  // namespace

BScan Rd3Reader::read(const std::filesystem::path& path, const ReadOptions& options) const {
    std::filesystem::path rd3_path = path;
    const std::string ext_up = upper_str(path.extension().string());
    if (ext_up == ".RAD") {
        rd3_path = find_companion_ext(path, ".rd3", ".RD3");
        if (rd3_path.empty()) {
            throw std::runtime_error("Missing companion .rd3 binary file for: " + path.string());
        }
    }

    const std::filesystem::path rad_path = find_companion_ext(rd3_path, ".rad", ".RAD");
    if (rad_path.empty()) {
        throw std::runtime_error("Missing companion MALA .rad header file for: " + rd3_path.string());
    }

    const auto kv = parse_rad_header(rad_path);
    auto get_num = [&](const std::string& k, double fallback) -> double {
        auto it = kv.find(k);
        if (it == kv.end() || it->second.empty()) {
            return fallback;
        }
        try {
            return std::stod(it->second);
        } catch (...) {
            return fallback;
        }
    };

    const auto samples_per_trace = static_cast<std::size_t>(std::lround(get_num("SAMPLES", 0.0)));
    if (samples_per_trace == 0) {
        throw std::runtime_error("Invalid MALA .rad header: SAMPLES missing or zero");
    }

    const double freq_mhz = get_num("FREQUENCY", 0.0);    // Sampling frequency in MHz
    const double time_window = get_num("TIMEWINDOW", 0.0); // Total trace window in ns
    const double dist_interval = get_num("DISTANCE INTERVAL", 0.02);
    const double time_interval = get_num("TIME INTERVAL", 0.02);

    double dt_ns = 0.1;
    if (options.override_dt_ns.has_value()) {
        dt_ns = *options.override_dt_ns;
    } else if (time_window > 0.0) {
        dt_ns = time_window / static_cast<double>(samples_per_trace);
    } else if (freq_mhz > 0.0) {
        // Sampling frequency in MHz -> period in ns = 1000 / freq_mhz
        dt_ns = 1000.0 / freq_mhz;
    }

    std::ifstream in(rd3_path, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error("Cannot open MALA .rd3 file: " + rd3_path.string());
    }
    const auto file_size = static_cast<std::size_t>(in.tellg());
    const std::size_t trace_bytes = samples_per_trace * sizeof(std::int16_t);
    const std::size_t num_traces = file_size / trace_bytes;

    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buf(num_traces * trace_bytes);
    if (!buf.empty() &&
        !in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()))) {
        throw std::runtime_error("Failed reading MALA .rd3 binary traces: " + rd3_path.string());
    }

    BScan bscan{};
    bscan.metadata.source_format = "MALA-RD3";
    bscan.metadata.samples_per_trace = samples_per_trace;
    bscan.metadata.sample_interval_ns = dt_ns;
    bscan.metadata.time_window_ns = dt_ns * static_cast<double>(samples_per_trace);
    bscan.metadata.trace_spacing_m = (dist_interval > 0.0) ? dist_interval : 0.02;
    bscan.metadata.scans_per_second = (time_interval > 0.0) ? (1.0 / time_interval) : 50.0;
    bscan.metadata.relative_permittivity = options.override_epsr.value_or(get_num("EPSR", 9.0));

    if (auto it = kv.find("ANTENNAS"); it != kv.end()) {
        bscan.metadata.description = "MALA RAMAC (" + it->second + ")";
        std::smatch m;
        static const std::regex re_mhz(R"(([0-9]{2,4})\s*M?H?z?)", std::regex::icase);
        if (std::regex_search(it->second, m, re_mhz)) {
            bscan.metadata.antenna_freq_mhz = std::stod(m[1].str());
        }
    } else {
        bscan.metadata.description = "MALA RAMAC RD3";
    }

    bscan.traces.reserve(num_traces);
    for (std::size_t i = 0; i < num_traces; ++i) {
        AScan tr{};
        tr.trace_id = i;
        tr.distance_along_track_m = static_cast<double>(i) * bscan.metadata.trace_spacing_m;
        tr.timestamp_sec = static_cast<double>(i) / bscan.metadata.scans_per_second;
        tr.samples.resize(samples_per_trace);

        const std::uint8_t* tptr = buf.data() + i * trace_bytes;
        for (std::size_t j = 0; j < samples_per_trace; ++j) {
            tr.samples[j] = static_cast<double>(detail::read_i16_le(tptr + j * 2));
        }
        bscan.traces.push_back(std::move(tr));
    }

    // Automatically load companion MALA .cor GPS file if present
    const std::filesystem::path cor_path = find_companion_ext(rd3_path, ".cor", ".COR");
    if (!cor_path.empty()) {
        const auto fixes = read_gps_file(cor_path);
        if (!fixes.empty()) {
            geo::synchronize_trajectory(bscan, fixes);
        }
    }

    return bscan;
}

void Rd3Reader::write(const std::filesystem::path& rd3_path, const BScan& bscan) {
    if (bscan.empty()) {
        throw std::invalid_argument("Rd3Reader::write: cannot write empty BScan");
    }

    std::ofstream out_rd3(rd3_path, std::ios::binary | std::ios::trunc);
    if (!out_rd3) {
        throw std::runtime_error("Cannot open MALA .rd3 file for writing: " + rd3_path.string());
    }

    const std::size_t ns = bscan.num_samples();
    double max_abs = 0.0;
    for (const auto& tr : bscan.traces) {
        for (double v : tr.samples) {
            max_abs = std::max(max_abs, std::abs(v));
        }
    }
    const double scale = (max_abs > 30000.0 || (max_abs > 0.0 && max_abs < 100.0))
                             ? (30000.0 / max_abs)
                             : 1.0;

    std::vector<std::uint8_t> tbuf(ns * 2);
    for (const auto& tr : bscan.traces) {
        for (std::size_t j = 0; j < ns; ++j) {
            const double val = (j < tr.samples.size()) ? (tr.samples[j] * scale) : 0.0;
            const auto s16 = static_cast<std::int16_t>(
                std::clamp(std::lround(val), -32767L, 32767L));
            detail::write_i16_le(tbuf.data() + j * 2, s16);
        }
        out_rd3.write(reinterpret_cast<const char*>(tbuf.data()), static_cast<std::streamsize>(tbuf.size()));
    }

    // Write companion .rad header
    auto rad_path = rd3_path;
    rad_path.replace_extension(".rad");
    std::ofstream out_rad(rad_path, std::ios::trunc);
    if (!out_rad) {
        throw std::runtime_error("Cannot open MALA .rad file for writing: " + rad_path.string());
    }

    const double samp_freq_mhz = 1000.0 / bscan.metadata.sample_interval_ns;
    const double time_window_ns = bscan.metadata.sample_interval_ns * static_cast<double>(ns);
    const double time_interval_s = (bscan.metadata.scans_per_second > 0.0)
                                       ? (1.0 / bscan.metadata.scans_per_second)
                                       : 0.02;

    out_rad << std::fixed << std::setprecision(6)
            << "SAMPLES:" << ns << "\n"
            << "FREQUENCY:" << samp_freq_mhz << "\n"
            << "FREQUENCY STEPS:1\n"
            << "SIGNAL POSITION:" << bscan.metadata.time_zero_ns << "\n"
            << "TIMEWINDOW:" << time_window_ns << "\n"
            << "LAST TRACE:" << bscan.num_traces() << "\n"
            << "DISTANCE INTERVAL:" << bscan.metadata.trace_spacing_m << "\n"
            << "TIME INTERVAL:" << time_interval_s << "\n"
            << "ANTENNAS:" << static_cast<int>(std::lround(bscan.metadata.antenna_freq_mhz)) << " MHz\n"
            << "ANTENNA SEPARATION:" << bscan.metadata.antenna_separation_m << "\n"
            << "EPSR:" << bscan.metadata.relative_permittivity << "\n";

    // Write companion .cor file if georeferenced
    const bool has_gps = std::any_of(
        bscan.traces.begin(), bscan.traces.end(),
        [](const AScan& t) { return t.position_wgs84.has_value(); });
    if (has_gps) {
        auto cor_path = rd3_path;
        cor_path.replace_extension(".cor");
        std::ofstream out_cor(cor_path, std::ios::trunc);
        if (out_cor) {
            for (std::size_t i = 0; i < bscan.traces.size(); ++i) {
                const auto& tr = bscan.traces[i];
                if (!tr.position_wgs84.has_value()) {
                    continue;
                }
                const double lat = tr.position_wgs84->latitude_deg;
                const double lon = tr.position_wgs84->longitude_deg;
                const char ns_hemi = (lat >= 0.0) ? 'N' : 'S';
                const char ew_hemi = (lon >= 0.0) ? 'E' : 'W';

                const double t_sec = tr.timestamp_sec.value_or(static_cast<double>(i) * time_interval_s);
                const int hh = static_cast<int>(t_sec / 3600.0) % 24;
                const int mm = static_cast<int>(std::fmod(t_sec, 3600.0) / 60.0);
                const double ss = std::fmod(t_sec, 60.0);

                out_cor << (i + 1) << "\t2026-11-07\t"
                        << std::setfill('0') << std::setw(2) << hh << ":"
                        << std::setw(2) << mm << ":"
                        << std::fixed << std::setw(5) << std::setprecision(2) << ss << "\t"
                        << std::setfill(' ') << std::setprecision(8) << std::abs(lat) << "\t" << ns_hemi << "\t"
                        << std::setprecision(8) << std::abs(lon) << "\t" << ew_hemi << "\t"
                        << std::setprecision(3) << tr.position_wgs84->altitude_m << "\tM\t4\n";
            }
        }
    }
}

}  // namespace gpr3d::io

