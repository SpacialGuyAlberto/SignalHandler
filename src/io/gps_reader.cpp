#include "gpr3d/io/gps_reader.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace gpr3d::io {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    return s.substr(start);
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::vector<std::string> split_char(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        out.push_back(trim(item));
    }
    if (!s.empty() && s.back() == delim) {
        out.emplace_back();
    }
    return out;
}

std::vector<std::string> split_whitespace(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        out.push_back(tok);
    }
    return out;
}

double parse_nmea_coord(const std::string& dm_str, const std::string& hemi) {
    const double val = std::stod(dm_str);
    const double deg = std::floor(val / 100.0);
    const double min = val - deg * 100.0;
    double dd = deg + min / 60.0;
    if (hemi == "S" || hemi == "W" || hemi == "s" || hemi == "w") {
        dd = -dd;
    }
    return dd;
}

std::optional<double> parse_hhmmss(const std::string& t_str) {
    if (t_str.size() < 6) {
        return std::nullopt;
    }
    try {
        // Supports both HHMMSS.ss (NMEA) and HH:MM:SS.ss (MALA .cor)
        if (t_str.find(':') != std::string::npos) {
            const auto parts = split_char(t_str, ':');
            if (parts.size() != 3) return std::nullopt;
            return std::stod(parts[0]) * 3600.0 + std::stod(parts[1]) * 60.0 + std::stod(parts[2]);
        }
        const double hh = std::stod(t_str.substr(0, 2));
        const double mm = std::stod(t_str.substr(2, 2));
        const double ss = std::stod(t_str.substr(4));
        return hh * 3600.0 + mm * 60.0 + ss;
    } catch (...) {
        return std::nullopt;
    }
}

bool verify_nmea_checksum(const std::string& line) {
    const auto star = line.find('*');
    if (star == std::string::npos || line.empty() || line[0] != '$') {
        return star == std::string::npos;  // Accept if checksum omitted
    }
    if (star + 3 > line.size()) {
        return false;
    }
    unsigned int computed = 0;
    for (std::size_t i = 1; i < star; ++i) {
        computed ^= static_cast<unsigned char>(line[i]);
    }
    try {
        const unsigned int expected = static_cast<unsigned int>(
            std::stoul(line.substr(star + 1, 2), nullptr, 16));
        return computed == expected;
    } catch (...) {
        return false;
    }
}

/// Parse a MALA .cor line:
/// <trace_1based> <YYYY-MM-DD> <HH:MM:SS.ss> <lat> <N/S> <lon> <E/W> <height> [M] [quality]
std::optional<GpsFix> try_parse_mala_cor_line(const std::string& line) {
    const auto toks = split_whitespace(line);
    if (toks.size() < 8) {
        return std::nullopt;
    }
    const std::string h_lat = to_lower(toks[4]);
    const std::string h_lon = to_lower(toks[6]);
    if ((h_lat != "n" && h_lat != "s") || (h_lon != "e" && h_lon != "w")) {
        return std::nullopt;
    }
    try {
        const long raw_idx = std::stol(toks[0]);
        double lat = std::stod(toks[3]);
        if (h_lat == "s") lat = -std::abs(lat);
        double lon = std::stod(toks[5]);
        if (h_lon == "w") lon = -std::abs(lon);
        const double alt = std::stod(toks[7]);
        int quality = 1;
        if (toks.size() >= 10) {
            quality = std::stoi(toks[9]);
        }
        GpsFix fix{};
        fix.trace_index = static_cast<std::size_t>(raw_idx > 0 ? raw_idx - 1 : 0);
        fix.timestamp_sec = parse_hhmmss(toks[2]);
        fix.wgs84 = Wgs84Coord{.latitude_deg = lat, .longitude_deg = lon, .altitude_m = alt};
        fix.fix_quality = quality;
        return fix;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace

std::optional<GpsFix> parse_nmea_gga(const std::string& raw_sentence) {
    const std::string line = trim(raw_sentence);
    if (line.size() < 15 || line[0] != '$') {
        return std::nullopt;
    }
    const std::string tag = line.substr(1, 5);
    if (tag != "GPGGA" && tag != "GNGGA" && tag != "GLGGA" && tag != "GAGGA") {
        return std::nullopt;
    }
    if (!verify_nmea_checksum(line)) {
        return std::nullopt;
    }

    const auto star = line.find('*');
    const std::string body = (star != std::string::npos) ? line.substr(0, star) : line;
    const auto fields = split_char(body, ',');
    if (fields.size() < 10) {
        return std::nullopt;
    }

    try {
        const int quality = fields[6].empty() ? 0 : std::stoi(fields[6]);
        if (quality <= 0 || fields[2].empty() || fields[4].empty()) {
            return std::nullopt;
        }
        const double lat = parse_nmea_coord(fields[2], fields[3]);
        const double lon = parse_nmea_coord(fields[4], fields[5]);
        const double ortho_alt = fields[9].empty() ? 0.0 : std::stod(fields[9]);

        GpsFix fix{};
        fix.timestamp_sec = parse_hhmmss(fields[1]);
        fix.wgs84 = Wgs84Coord{
            .latitude_deg = lat,
            .longitude_deg = lon,
            .altitude_m = ortho_alt
        };
        fix.fix_quality = quality;
        return fix;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<GpsFix> read_gps_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Cannot open GPS file: " + path.string());
    }

    std::vector<GpsFix> fixes;
    std::string line;

    // Optional CSV column mapping
    bool header_checked = false;
    int col_trace = -1;
    int col_time = -1;
    int col_lat = -1;
    int col_lon = -1;
    int col_alt = -1;
    char csv_delim = ',';

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // 1. NMEA sentence?
        if (line[0] == '$') {
            if (auto fix = parse_nmea_gga(line)) {
                fixes.push_back(*fix);
            }
            continue;
        }

        // 2. MALA .cor line?
        if (auto cor_fix = try_parse_mala_cor_line(line)) {
            fixes.push_back(*cor_fix);
            continue;
        }

        // 3. CSV / semicolon-separated table
        if (!header_checked) {
            header_checked = true;
            if (line.find(';') != std::string::npos && line.find(',') == std::string::npos) {
                csv_delim = ';';
            }
            const auto cols = split_char(line, csv_delim);
            bool has_alpha = false;
            for (const auto& c : cols) {
                for (unsigned char ch : c) {
                    if (std::isalpha(ch) && ch != 'e' && ch != 'E') {
                        has_alpha = true;
                        break;
                    }
                }
            }
            if (has_alpha) {
                for (std::size_t i = 0; i < cols.size(); ++i) {
                    const std::string cname = to_lower(cols[i]);
                    if (cname.find("trace") != std::string::npos || cname == "scan" || cname == "id") {
                        col_trace = static_cast<int>(i);
                    } else if (cname.find("time") != std::string::npos || cname == "t") {
                        col_time = static_cast<int>(i);
                    } else if (cname.find("lat") != std::string::npos) {
                        col_lat = static_cast<int>(i);
                    } else if (cname.find("lon") != std::string::npos || cname.find("lng") != std::string::npos) {
                        col_lon = static_cast<int>(i);
                    } else if (cname.find("alt") != std::string::npos ||
                               cname.find("elev") != std::string::npos ||
                               cname.find("height") != std::string::npos || cname == "z") {
                        col_alt = static_cast<int>(i);
                    }
                }
                continue;  // Header consumed
            } else {
                // Default positional layout without header:
                // 3 cols -> lat, lon, alt
                // 4+ cols -> trace_index, lat, lon, alt
                if (cols.size() == 2) {
                    col_lat = 0;
                    col_lon = 1;
                } else if (cols.size() == 3) {
                    col_lat = 0;
                    col_lon = 1;
                    col_alt = 2;
                } else if (cols.size() >= 4) {
                    col_trace = 0;
                    col_lat = 1;
                    col_lon = 2;
                    col_alt = 3;
                }
            }
        }

        const auto cols = split_char(line, csv_delim);
        if (col_lat < 0 || col_lon < 0 ||
            static_cast<std::size_t>(std::max(col_lat, col_lon)) >= cols.size()) {
            continue;
        }

        try {
            GpsFix fix{};
            if (col_trace >= 0 && static_cast<std::size_t>(col_trace) < cols.size() &&
                !cols[static_cast<std::size_t>(col_trace)].empty()) {
                fix.trace_index = static_cast<std::size_t>(
                    std::stoull(cols[static_cast<std::size_t>(col_trace)]));
            }
            if (col_time >= 0 && static_cast<std::size_t>(col_time) < cols.size() &&
                !cols[static_cast<std::size_t>(col_time)].empty()) {
                fix.timestamp_sec = std::stod(cols[static_cast<std::size_t>(col_time)]);
            }
            fix.wgs84.latitude_deg = std::stod(cols[static_cast<std::size_t>(col_lat)]);
            fix.wgs84.longitude_deg = std::stod(cols[static_cast<std::size_t>(col_lon)]);
            if (col_alt >= 0 && static_cast<std::size_t>(col_alt) < cols.size() &&
                !cols[static_cast<std::size_t>(col_alt)].empty()) {
                fix.wgs84.altitude_m = std::stod(cols[static_cast<std::size_t>(col_alt)]);
            }
            fixes.push_back(fix);
        } catch (...) {
            continue;
        }
    }

    return fixes;
}

}  // namespace gpr3d::io

