#include "gpr3d/io/segy_reader.hpp"
#include "gpr3d/core/endian.hpp"
#include "gpr3d/geo/crs_transform.hpp"
#include "gpr3d/geo/trajectory_sync.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gpr3d::io {

namespace {

double apply_segy_scalar(double raw, std::int16_t scalar) noexcept {
    if (scalar > 0) {
        return raw * static_cast<double>(scalar);
    }
    if (scalar < 0) {
        return raw / static_cast<double>(-scalar);
    }
    return raw;
}

std::size_t bytes_per_sample_for_format(std::uint16_t fmt) {
    switch (fmt) {
        case 1:  // 4-byte IBM float
        case 2:  // 4-byte int32
        case 5:  // 4-byte IEEE float
            return 4;
        case 3:  // 2-byte int16
            return 2;
        case 8:  // 1-byte int8
            return 1;
        default:
            throw std::runtime_error("Unsupported SEG-Y data sample format code: " + std::to_string(fmt));
    }
}

void parse_metadata_hints_from_text_header(const std::string& text, SurveyMetadata& meta) {
    // Extract optional GPR parameters such as EPSR=9.0 or ANTENNA_MHZ=400 embedded in textual cards
    std::smatch m;
    static const std::regex re_epsr(R"(EPSR\s*[=:]\s*([0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    if (std::regex_search(text, m, re_epsr)) {
        meta.relative_permittivity = std::stod(m[1].str());
    }
    static const std::regex re_ant(R"(ANTENNA_MHZ\s*[=:]\s*([0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    if (std::regex_search(text, m, re_ant)) {
        meta.antenna_freq_mhz = std::stod(m[1].str());
    }
    static const std::regex re_t0(R"(TIME_ZERO_NS\s*[=:]\s*(-?[0-9]+(?:\.[0-9]+)?))", std::regex::icase);
    if (std::regex_search(text, m, re_t0)) {
        meta.time_zero_ns = std::stod(m[1].str());
    }
}

}  // namespace

BScan SegyReader::read(const std::filesystem::path& path, const ReadOptions& options) const {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        throw std::runtime_error("Cannot open SEG-Y file: " + path.string());
    }
    const auto file_size = static_cast<std::size_t>(in.tellg());
    if (file_size < 3600) {
        throw std::runtime_error("Invalid SEG-Y file (< 3600 bytes): " + path.string());
    }

    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buf(file_size);
    if (!in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(file_size))) {
        throw std::runtime_error("Failed reading SEG-Y file: " + path.string());
    }

    BScan bscan{};
    bscan.metadata.source_format = "SEG-Y";
    bscan.metadata.description = detail::decode_segy_text_header(buf.data(), 3200);
    parse_metadata_hints_from_text_header(bscan.metadata.description, bscan.metadata);

    const std::uint8_t* bin = buf.data() + 3200;
    const std::uint16_t fmt_be = detail::read_u16_be(bin + 24);
    const std::uint16_t fmt_le = detail::read_u16_le(bin + 24);

    bool big_endian = true;
    std::uint16_t fmt_code = fmt_be;
    if (fmt_be == 1 || fmt_be == 2 || fmt_be == 3 || fmt_be == 5 || fmt_be == 8) {
        big_endian = true;
        fmt_code = fmt_be;
    } else if (fmt_le == 1 || fmt_le == 2 || fmt_le == 3 || fmt_le == 5 || fmt_le == 8) {
        big_endian = false;
        fmt_code = fmt_le;
    } else {
        throw std::runtime_error("Unrecognized SEG-Y sample format code in binary header");
    }

    auto rd_u16 = [big_endian](const std::uint8_t* p) {
        return big_endian ? detail::read_u16_be(p) : detail::read_u16_le(p);
    };
    auto rd_i16 = [big_endian](const std::uint8_t* p) {
        return big_endian ? detail::read_i16_be(p) : detail::read_i16_le(p);
    };
    auto rd_u32 = [big_endian](const std::uint8_t* p) {
        return big_endian ? detail::read_u32_be(p) : detail::read_u32_le(p);
    };
    auto rd_i32 = [big_endian](const std::uint8_t* p) {
        return big_endian ? detail::read_i32_be(p) : detail::read_i32_le(p);
    };
    auto rd_f32 = [big_endian](const std::uint8_t* p) {
        return big_endian ? detail::read_f32_be(p) : detail::read_f32_le(p);
    };

    const std::uint16_t raw_dt = rd_u16(bin + 16);
    const std::uint16_t global_ns = rd_u16(bin + 20);
    const std::int16_t ext_headers = rd_i16(bin + 304);

    double dt_ns = 0.1;
    if (options.override_dt_ns.has_value()) {
        dt_ns = *options.override_dt_ns;
    } else if (raw_dt > 0) {
        // GPR SEG-Y convention stores picoseconds (1e-3 ns), or femtoseconds if >= 10000
        dt_ns = (raw_dt >= 10000) ? (static_cast<double>(raw_dt) * 1e-6)
                                  : (static_cast<double>(raw_dt) * 1e-3);
    }
    if (options.override_epsr.has_value()) {
        bscan.metadata.relative_permittivity = *options.override_epsr;
    }

    bscan.metadata.sample_interval_ns = dt_ns;
    bscan.metadata.samples_per_trace = global_ns;
    bscan.metadata.time_window_ns = dt_ns * static_cast<double>(global_ns);

    std::size_t offset = 3600;
    if (ext_headers > 0) {
        const std::size_t skip = static_cast<std::size_t>(ext_headers) * 3200;
        if (offset + skip < file_size) {
            offset += skip;
        }
    }

    const std::size_t sample_bytes = bytes_per_sample_for_format(fmt_code);
    std::size_t trace_idx = 0;
    bool any_coords = false;

    while (offset + 240 <= file_size) {
        const std::uint8_t* th = buf.data() + offset;
        std::uint16_t ns = rd_u16(th + 114);
        if (ns == 0) {
            ns = global_ns;
        }
        if (ns == 0) {
            break;
        }

        const std::size_t data_bytes = static_cast<std::size_t>(ns) * sample_bytes;
        if (offset + 240 + data_bytes > file_size) {
            break;
        }

        AScan trace{};
        trace.trace_id = trace_idx;
        trace.distance_along_track_m = static_cast<double>(trace_idx) * bscan.metadata.trace_spacing_m;
        trace.samples.resize(ns);

        const std::uint8_t* sptr = th + 240;
        for (std::uint16_t j = 0; j < ns; ++j) {
            const std::uint8_t* p = sptr + static_cast<std::size_t>(j) * sample_bytes;
            switch (fmt_code) {
                case 1:
                    trace.samples[j] = detail::ibm_to_double(rd_u32(p));
                    break;
                case 2:
                    trace.samples[j] = static_cast<double>(rd_i32(p));
                    break;
                case 3:
                    trace.samples[j] = static_cast<double>(rd_i16(p));
                    break;
                case 5:
                    trace.samples[j] = static_cast<double>(rd_f32(p));
                    break;
                case 8:
                    trace.samples[j] = static_cast<double>(static_cast<std::int8_t>(*p));
                    break;
                default:
                    break;
            }
        }

        // Extract embedded trace coordinates if present
        const std::int32_t sx = rd_i32(th + 72);
        const std::int32_t sy = rd_i32(th + 76);
        if (sx != 0 || sy != 0) {
            const std::int16_t coord_scalar = rd_i16(th + 70);
            const std::int16_t elev_scalar = rd_i16(th + 68);
            const std::int16_t counit = rd_i16(th + 88);
            std::int32_t raw_elev = rd_i32(th + 44);
            if (raw_elev == 0) {
                raw_elev = rd_i32(th + 40);
            }

            const double x = apply_segy_scalar(static_cast<double>(sx), coord_scalar);
            const double y = apply_segy_scalar(static_cast<double>(sy), coord_scalar);
            const double alt_m = apply_segy_scalar(static_cast<double>(raw_elev), elev_scalar);

            if (counit == 2) {
                // Seconds of arc
                trace.position_wgs84 = Wgs84Coord{
                    .latitude_deg = y / 3600.0,
                    .longitude_deg = x / 3600.0,
                    .altitude_m = alt_m
                };
                any_coords = true;
            } else if (counit == 3 ||
                       (counit != 1 && std::abs(x) <= 180.0 && std::abs(y) <= 90.0)) {
                trace.position_wgs84 = Wgs84Coord{
                    .latitude_deg = y,
                    .longitude_deg = x,
                    .altitude_m = alt_m
                };
                any_coords = true;
            } else if (options.utm_zone.has_value()) {
                trace.position_wgs84 = geo::utm_to_wgs84(UtmCoord{
                    .easting_m = x,
                    .northing_m = y,
                    .elevation_m = alt_m,
                    .zone = *options.utm_zone,
                    .is_northern = options.utm_northern
                });
                any_coords = true;
            }
        }

        bscan.traces.push_back(std::move(trace));
        offset += 240 + data_bytes;
        ++trace_idx;
    }

    if (!bscan.traces.empty()) {
        bscan.metadata.samples_per_trace = bscan.traces.front().samples.size();
        bscan.metadata.time_window_ns =
            bscan.metadata.sample_interval_ns * static_cast<double>(bscan.metadata.samples_per_trace);
    }

    if (any_coords) {
        geo::update_headings_and_distances(bscan);
    }

    return bscan;
}

void SegyReader::write(const std::filesystem::path& path, const BScan& bscan) {
    if (bscan.empty()) {
        throw std::invalid_argument("SegyReader::write: cannot write empty BScan");
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Cannot open SEG-Y file for writing: " + path.string());
    }

    // 1. Build 3200-byte ASCII Textual File Header (40 cards x 80 characters)
    std::string text_hdr(3200, ' ');
    auto set_card = [&](int card_idx, const std::string& content) {
        char prefix[8];
        std::snprintf(prefix, sizeof(prefix), "C%02d ", card_idx + 1);
        const std::string full = std::string(prefix) + content;
        const std::size_t copy_len = std::min<std::size_t>(full.size(), 80);
        std::memcpy(text_hdr.data() + static_cast<std::size_t>(card_idx) * 80, full.data(), copy_len);
    };

    set_card(0, "CLIENT: GPR3D SIGNALHANDLER   SURVEY: GEOREFERENCED GPR PROFILE");
    set_card(1, "FORMAT: SEG-Y REV 1 IEEE FLOAT32 (BIG-ENDIAN)");
    set_card(2, "SAMPLE INTERVAL STORED IN PICOSECONDS; COORDS IN ARC-SECONDS (SCALAR -10000)");
    {
        std::ostringstream oss;
        oss << "EPSR=" << bscan.metadata.relative_permittivity
            << " ANTENNA_MHZ=" << bscan.metadata.antenna_freq_mhz
            << " TIME_ZERO_NS=" << bscan.metadata.time_zero_ns;
        set_card(3, oss.str());
    }
    set_card(39, "END TEXTUAL HEADER");
    out.write(text_hdr.data(), 3200);

    // 2. Build 400-byte Binary File Header
    std::vector<std::uint8_t> bin(400, 0);
    const auto ns = static_cast<std::uint16_t>(bscan.num_samples());
    const auto dt_ps = static_cast<std::uint16_t>(
        std::clamp(std::lround(bscan.metadata.sample_interval_ns * 1000.0), 1L, 65535L));

    detail::write_u32_be(bin.data() + 0, 1);       // Job ID
    detail::write_u32_be(bin.data() + 4, 1);       // Line number
    detail::write_u16_be(bin.data() + 12, 1);      // Data traces per ensemble
    detail::write_u16_be(bin.data() + 16, dt_ps);  // Sample interval (ps)
    detail::write_u16_be(bin.data() + 20, ns);     // Samples per trace
    detail::write_u16_be(bin.data() + 24, 5);      // Format 5 = 4-byte IEEE float
    detail::write_u16_be(bin.data() + 54, 1);      // Measurement system: 1 = Meters
    detail::write_u16_be(bin.data() + 300, 0x0100);  // SEG-Y Rev 1.0
    detail::write_u16_be(bin.data() + 302, 1);     // Fixed-length trace flag
    out.write(reinterpret_cast<const char*>(bin.data()), 400);

    // 3. Write Traces (240-byte trace header + ns * float32)
    std::vector<std::uint8_t> th(240, 0);
    std::vector<std::uint8_t> sbuf(static_cast<std::size_t>(ns) * 4, 0);

    for (std::size_t i = 0; i < bscan.traces.size(); ++i) {
        std::fill(th.begin(), th.end(), 0);
        const auto& tr = bscan.traces[i];
        const auto seq = static_cast<std::int32_t>(i + 1);

        detail::write_i32_be(th.data() + 0, seq);    // Trace sequence within line
        detail::write_i32_be(th.data() + 4, seq);    // Trace sequence within file
        detail::write_i16_be(th.data() + 28, 1);     // Trace ID code: 1 = seismic/radar data
        detail::write_u16_be(th.data() + 114, ns);
        detail::write_u16_be(th.data() + 116, dt_ps);

        if (tr.position_wgs84.has_value()) {
            // Store arc-seconds * 10000 (~0.3 cm resolution worldwide, fits in int32: 180*3600*10000 = 6.48e9 -> use -1000 for lon up to 180: 180*3600*1000 = 6.48e8 < 2.14e9!)
            // Wait: 180 * 3600 * 1000 = 648,000,000, or * 3000 -> use -1000? Let's check:
            // With counit = 3 (decimal degrees) and scalar = -10000000 (1e-7 deg = 1.1 cm), max = 180 * 1e7 = 1.8e9 < 2.147e9!
            // With counit = 2 (arc-seconds) and scalar = -1000 (1e-3 arcsec = 3.0 cm), max = 6.48e8 < 2.147e9!
            // Actually with counit = 3 and scalar = -10000000 (1e-7 deg = 1.1 cm), or counit = 2 and scalar = -3000? Standard SEG-Y scalars must be powers of 10: +/- 1, 10, 100, 1000, 10000!
            // Wait: in SEG-Y spec, byte 71-72 scalar is restricted to +/- 1, 10, 100, 1000, 10000!
            // With counit = 2 (arc-seconds) and scalar = -1000: 1e-3 arcsec = 0.03 m, and for |lon| < 59 deg (all of Europe/Americas? No, Americas is 120W), -1000 fits ALL +/-180 longitudes (6.48e8 < 2.147e9)! And for |lon| < 59 deg, scalar = -10000 gives 180? 59*3600*10000 = 2.124e9 < 2.147e9 -> 3 mm precision!
            const double max_abs_deg = std::max(
                std::abs(tr.position_wgs84->longitude_deg),
                std::abs(tr.position_wgs84->latitude_deg));
            const std::int16_t coord_scalar = (max_abs_deg < 55.0) ? -10000 : -1000;
            const double mult = static_cast<double>(-coord_scalar);

            const auto sx = static_cast<std::int32_t>(
                std::llround(tr.position_wgs84->longitude_deg * 3600.0 * mult));
            const auto sy = static_cast<std::int32_t>(
                std::llround(tr.position_wgs84->latitude_deg * 3600.0 * mult));
            const auto selev = static_cast<std::int32_t>(
                std::llround(tr.position_wgs84->altitude_m * 1000.0));

            detail::write_i32_be(th.data() + 44, selev);
            detail::write_i16_be(th.data() + 68, -1000);         // mm elevation precision
            detail::write_i16_be(th.data() + 70, coord_scalar);  // arcsec precision
            detail::write_i32_be(th.data() + 72, sx);
            detail::write_i32_be(th.data() + 76, sy);
            detail::write_i16_be(th.data() + 88, 2);             // 2 = Seconds of arc
        }

        out.write(reinterpret_cast<const char*>(th.data()), 240);

        for (std::uint16_t j = 0; j < ns; ++j) {
            const float val = (j < tr.samples.size()) ? static_cast<float>(tr.samples[j]) : 0.0f;
            detail::write_f32_be(sbuf.data() + static_cast<std::size_t>(j) * 4, val);
        }
        out.write(reinterpret_cast<const char*>(sbuf.data()), static_cast<std::streamsize>(sbuf.size()));
    }
}

}  // namespace gpr3d::io

