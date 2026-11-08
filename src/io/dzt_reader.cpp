#include "gpr3d/io/dzt_reader.hpp"
#include "gpr3d/core/endian.hpp"
#include "gpr3d/geo/trajectory_sync.hpp"
#include "gpr3d/io/gps_reader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace gpr3d::io {

namespace {

constexpr std::size_t kMinHeadSize = 1024;

std::vector<GpsFix> parse_dzg_file(const std::filesystem::path& dzg_path) {
    std::ifstream in(dzg_path);
    if (!in) {
        return {};
    }
    std::vector<GpsFix> fixes;
    std::optional<std::size_t> pending_scan;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("$GSSIS,", 0) == 0 || line.rfind("$GSSI,", 0) == 0) {
            const auto c1 = line.find(',');
            const auto c2 = line.find(',', c1 + 1);
            if (c1 != std::string::npos) {
                try {
                    const std::string num_str = line.substr(
                        c1 + 1, (c2 != std::string::npos) ? (c2 - c1 - 1) : std::string::npos);
                    pending_scan = static_cast<std::size_t>(std::stoull(num_str));
                } catch (...) {
                    pending_scan.reset();
                }
            }
        } else if (!line.empty() && line[0] == '$') {
            if (auto fix = parse_nmea_gga(line)) {
                if (pending_scan.has_value()) {
                    fix->trace_index = *pending_scan;
                    pending_scan.reset();
                }
                fixes.push_back(*fix);
            }
        }
    }
    return fixes;
}

}  // namespace

BScan DztReader::read(const std::filesystem::path& path, const ReadOptions& options) const {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error("Cannot open GSSI DZT file: " + path.string());
    }
    const auto file_size = static_cast<std::size_t>(in.tellg());
    if (file_size < kMinHeadSize) {
        throw std::runtime_error("Invalid GSSI DZT file (< 1024 bytes): " + path.string());
    }

    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buf(file_size);
    if (!in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(file_size))) {
        throw std::runtime_error("Failed reading GSSI DZT file: " + path.string());
    }

    const std::uint8_t* hdr = buf.data();
    const std::uint16_t rh_data = detail::read_u16_le(hdr + 2);
    const std::uint16_t rh_nsamp = detail::read_u16_le(hdr + 4);
    const std::uint16_t rh_bits = detail::read_u16_le(hdr + 6);
    const float rhf_sps = detail::read_f32_le(hdr + 10);
    const float rhf_spm = detail::read_f32_le(hdr + 14);
    const float rhf_position = detail::read_f32_le(hdr + 22);
    const float rhf_range = detail::read_f32_le(hdr + 26);
    const float rhf_epsr = detail::read_f32_le(hdr + 54);

    if (rh_nsamp == 0) {
        throw std::runtime_error("Invalid GSSI DZT header: rh_nsamp is 0");
    }
    if (rh_bits != 8 && rh_bits != 16 && rh_bits != 32) {
        throw std::runtime_error("Unsupported GSSI DZT bit depth: " + std::to_string(rh_bits));
    }

    std::size_t data_offset = kMinHeadSize;
    if (rh_data > 0) {
        data_offset = (rh_data < kMinHeadSize)
                          ? (static_cast<std::size_t>(rh_data) * kMinHeadSize)
                          : static_cast<std::size_t>(rh_data);
    }
    if (data_offset >= file_size) {
        throw std::runtime_error("Invalid GSSI DZT header: data_offset exceeds file size");
    }

    // Parse antenna name at byte offset 98 (14 chars)
    char ant_buf[15]{};
    std::memcpy(ant_buf, hdr + 98, 14);
    const std::string ant_name(ant_buf);

    BScan bscan{};
    bscan.metadata.source_format = "GSSI-DZT";
    bscan.metadata.samples_per_trace = rh_nsamp;
    bscan.metadata.time_window_ns = (rhf_range > 0.0f) ? static_cast<double>(rhf_range) : 50.0;
    bscan.metadata.sample_interval_ns = options.override_dt_ns.value_or(
        bscan.metadata.time_window_ns / static_cast<double>(rh_nsamp));
    bscan.metadata.time_zero_ns = (std::isfinite(rhf_position) && rhf_position >= 0.0f)
                                      ? static_cast<double>(rhf_position)
                                      : 0.0;
    bscan.metadata.scans_per_second = (rhf_sps > 0.0f) ? static_cast<double>(rhf_sps) : 50.0;
    bscan.metadata.trace_spacing_m = (rhf_spm > 0.0f) ? (1.0 / static_cast<double>(rhf_spm)) : 0.02;
    bscan.metadata.relative_permittivity = options.override_epsr.value_or(
        (rhf_epsr > 0.5f && rhf_epsr < 100.0f) ? static_cast<double>(rhf_epsr) : 9.0);
    bscan.metadata.description = "GSSI RADAN DZT (" + ant_name + ")";

    // Extract numeric MHz from antenna string if present (e.g. "400MHz")
    std::smatch m;
    static const std::regex re_mhz(R"(([0-9]{2,4})\s*M?H?z?)", std::regex::icase);
    if (std::regex_search(ant_name, m, re_mhz)) {
        bscan.metadata.antenna_freq_mhz = std::stod(m[1].str());
    }

    const std::size_t bytes_per_sample = rh_bits / 8;
    const std::size_t trace_bytes = static_cast<std::size_t>(rh_nsamp) * bytes_per_sample;
    const std::size_t num_traces = (file_size - data_offset) / trace_bytes;

    bscan.traces.reserve(num_traces);
    for (std::size_t i = 0; i < num_traces; ++i) {
        AScan tr{};
        tr.trace_id = i;
        tr.distance_along_track_m = static_cast<double>(i) * bscan.metadata.trace_spacing_m;
        if (bscan.metadata.scans_per_second > 0.0) {
            tr.timestamp_sec = static_cast<double>(i) / bscan.metadata.scans_per_second;
        }
        tr.samples.resize(rh_nsamp);

        const std::uint8_t* tptr = buf.data() + data_offset + i * trace_bytes;
        for (std::uint16_t j = 0; j < rh_nsamp; ++j) {
            const std::uint8_t* p = tptr + static_cast<std::size_t>(j) * bytes_per_sample;
            if (rh_bits == 8) {
                tr.samples[j] = static_cast<double>(*p) - 128.0;
            } else if (rh_bits == 16) {
                tr.samples[j] = static_cast<double>(detail::read_u16_le(p)) - 32768.0;
            } else {
                tr.samples[j] = static_cast<double>(detail::read_i32_le(p));
            }
        }
        bscan.traces.push_back(std::move(tr));
    }

    // Automatically load companion .dzg file if present
    auto dzg_path = path;
    dzg_path.replace_extension(".dzg");
    if (std::filesystem::exists(dzg_path)) {
        const auto fixes = parse_dzg_file(dzg_path);
        if (!fixes.empty()) {
            geo::synchronize_trajectory(bscan, fixes);
        }
    }

    return bscan;
}

void DztReader::write(const std::filesystem::path& path, const BScan& bscan) {
    if (bscan.empty()) {
        throw std::invalid_argument("DztReader::write: cannot write empty BScan");
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open DZT file for writing: " + path.string());
    }

    const auto ns = static_cast<std::uint16_t>(bscan.num_samples());
    std::vector<std::uint8_t> hdr(kMinHeadSize, 0);

    const float sps = static_cast<float>(
        bscan.metadata.scans_per_second > 0.0 ? bscan.metadata.scans_per_second : 50.0);
    const float spm = static_cast<float>(
        bscan.metadata.trace_spacing_m > 0.0 ? (1.0 / bscan.metadata.trace_spacing_m) : 50.0);
    const float pos_ns = static_cast<float>(bscan.metadata.time_zero_ns);
    const float range_ns = static_cast<float>(
        bscan.metadata.sample_interval_ns * static_cast<double>(ns));
    const float epsr = static_cast<float>(bscan.metadata.relative_permittivity);
    const float v_m_ns = static_cast<float>(
        kSpeedOfLightMPerNs / std::sqrt(std::max(1.0, bscan.metadata.relative_permittivity)));
    const float depth_m = 0.5f * v_m_ns * range_ns;

    detail::write_u16_le(hdr.data() + 0, 0x00FF);          // rh_tag
    detail::write_u16_le(hdr.data() + 2, kMinHeadSize);    // rh_data = 1024
    detail::write_u16_le(hdr.data() + 4, ns);              // rh_nsamp
    detail::write_u16_le(hdr.data() + 6, 16);              // rh_bits = 16-bit unsigned offset binary
    detail::write_i16_le(hdr.data() + 8, -32768);          // rh_zero
    detail::write_f32_le(hdr.data() + 10, sps);            // rhf_sps
    detail::write_f32_le(hdr.data() + 14, spm);            // rhf_spm
    detail::write_f32_le(hdr.data() + 22, pos_ns);         // rhf_position
    detail::write_f32_le(hdr.data() + 26, range_ns);       // rhf_range
    detail::write_u16_le(hdr.data() + 30, 1);              // rh_npass
    detail::write_f32_le(hdr.data() + 54, epsr);           // rhf_epsr
    detail::write_f32_le(hdr.data() + 58, 0.0f);           // rhf_top
    detail::write_f32_le(hdr.data() + 62, depth_m);        // rhf_depth
    detail::write_u16_le(hdr.data() + 94, 1);              // rh_nchan

    char ant[15]{};
    std::snprintf(ant, sizeof(ant), "%dMHz", static_cast<int>(std::lround(bscan.metadata.antenna_freq_mhz)));
    std::memcpy(hdr.data() + 98, ant, std::min<std::size_t>(std::strlen(ant), 14));

    out.write(reinterpret_cast<const char*>(hdr.data()), kMinHeadSize);

    // Find peak absolute amplitude to normalize cleanly into 16-bit dynamic range [-30000, +30000]
    double max_abs = 0.0;
    for (const auto& tr : bscan.traces) {
        for (double v : tr.samples) {
            max_abs = std::max(max_abs, std::abs(v));
        }
    }
    const double scale = (max_abs > 30000.0 || (max_abs > 0.0 && max_abs < 100.0))
                             ? (30000.0 / max_abs)
                             : 1.0;

    std::vector<std::uint8_t> tbuf(static_cast<std::size_t>(ns) * 2);
    for (const auto& tr : bscan.traces) {
        for (std::uint16_t j = 0; j < ns; ++j) {
            const double raw = (j < tr.samples.size()) ? (tr.samples[j] * scale) : 0.0;
            const double shifted = std::clamp(std::round(raw + 32768.0), 0.0, 65535.0);
            detail::write_u16_le(
                tbuf.data() + static_cast<std::size_t>(j) * 2,
                static_cast<std::uint16_t>(shifted));
        }
        out.write(reinterpret_cast<const char*>(tbuf.data()), static_cast<std::streamsize>(tbuf.size()));
    }
}

}  // namespace gpr3d::io

