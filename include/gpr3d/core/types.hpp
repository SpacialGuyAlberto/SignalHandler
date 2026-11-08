#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace gpr3d {

/// Physical speed of light in vacuum in meters per nanosecond (m/ns).
inline constexpr double kSpeedOfLightMPerNs = 0.299792458;

/// Geodetic coordinates on the WGS84 ellipsoid.
struct Wgs84Coord {
    double latitude_deg{0.0};   ///< Geodetic latitude in decimal degrees [-90, 90]
    double longitude_deg{0.0};  ///< Geodetic longitude in decimal degrees [-180, 180]
    double altitude_m{0.0};     ///< Ellipsoidal / orthometric elevation in meters
};

/// Earth-Centered, Earth-Fixed (ECEF) Cartesian coordinates in meters (WGS84).
struct EcefCoord {
    double x_m{0.0};
    double y_m{0.0};
    double z_m{0.0};
};

/// Local tangent plane East-North-Up (ENU) metric coordinates relative to a reference origin.
struct EnuCoord {
    double east_m{0.0};
    double north_m{0.0};
    double up_m{0.0};
};

/// Universal Transverse Mercator (UTM / ETRS89) projected metric coordinates.
struct UtmCoord {
    double easting_m{500000.0};
    double northing_m{0.0};
    double elevation_m{0.0};
    int zone{32};
    bool is_northern{true};

    [[nodiscard]] int epsg_code() const noexcept {
        return (is_northern ? 32600 : 32700) + zone;
    }

    [[nodiscard]] std::string zone_string() const {
        return std::to_string(zone) + (is_northern ? "N" : "S");
    }
};

/// Single GNSS navigation fix used to georeference GPR traces.
struct GpsFix {
    std::optional<std::size_t> trace_index;  ///< Direct 0-based trace index if logged per scan
    std::optional<double> timestamp_sec;     ///< UTC or survey-relative timestamp in seconds
    Wgs84Coord wgs84{};                      ///< Geodetic antenna/rover position
    int fix_quality{1};                      ///< NMEA quality (1=GPS, 2=DGPS, 4=RTK Fixed, 5=RTK Float)
};

/// Single 1D radar vertical sounding (A-scan / trace).
struct AScan {
    std::size_t trace_id{0};
    double distance_along_track_m{0.0};
    std::optional<double> timestamp_sec;
    std::optional<Wgs84Coord> position_wgs84;
    double heading_deg{0.0};       ///< Survey azimuth clockwise from true North [0, 360)
    std::vector<double> samples;   ///< Processed or raw radar waveform samples
    std::vector<double> envelope;  ///< Instantaneous Hilbert amplitude envelope (if computed)
};

/// Acquisition & medium parameters describing a GPR profile (B-scan).
struct SurveyMetadata {
    std::string source_format{"UNKNOWN"};
    double sample_interval_ns{0.1};     ///< Sampling step dt in nanoseconds
    double time_window_ns{50.0};        ///< Total trace recording window in nanoseconds
    double time_zero_ns{0.0};           ///< Time-zero (air/ground surface arrival) offset in ns
    std::size_t samples_per_trace{0};   ///< Number of vertical samples per A-scan
    double trace_spacing_m{0.02};       ///< Nominal horizontal distance step between traces (m)
    double scans_per_second{50.0};      ///< Pulse repetition / scan rate (Hz)
    double antenna_freq_mhz{400.0};     ///< Nominal center frequency of GPR antenna (MHz)
    double antenna_separation_m{0.0};   ///< Tx-Rx bistatic offset in meters (0 = monostatic)
    double relative_permittivity{9.0};  ///< Subsurface dielectric constant eps_r (default 9 -> ~0.10 m/ns)
    std::string description{};
};

/// 2D radar profile (B-scan) composed of sequential A-scans along a trajectory.
struct BScan {
    SurveyMetadata metadata{};
    std::vector<AScan> traces{};

    [[nodiscard]] std::size_t num_traces() const noexcept { return traces.size(); }
    [[nodiscard]] std::size_t num_samples() const noexcept {
        return traces.empty() ? metadata.samples_per_trace : traces.front().samples.size();
    }
    [[nodiscard]] bool empty() const noexcept { return traces.empty(); }
};

/// Georeferenced 3D subsurface point produced by interpreting a GPR radargram.
struct Point3D {
    Wgs84Coord wgs84{};     ///< WGS84 geodetic coordinates (altitude_m = surface_alt - depth_m)
    UtmCoord utm{};         ///< Projected metric UTM / ETRS89 coordinates
    EnuCoord enu{};         ///< Local metric East-North-Up coordinates relative to cloud origin

    double depth_m{0.0};    ///< Depth below ground surface in meters (positive downward)
    double twt_ns{0.0};     ///< Corrected two-way travel time in nanoseconds
    double amplitude{0.0};  ///< Signed processed radar amplitude
    double envelope{0.0};   ///< Instantaneous Hilbert envelope magnitude
    float intensity{0.0f};  ///< Normalized reflectance intensity in [0.0, 1.0]

    std::size_t trace_id{0};
    std::size_t sample_idx{0};
    bool is_peak{false};    ///< True if extracted as a local subsurface reflector peak
};

/// Collection of georeferenced 3D radar reflection points with survey reference origin.
struct PointCloud3D {
    Wgs84Coord origin_wgs84{};
    UtmCoord origin_utm{};
    bool has_valid_crs{false};
    std::string source_description{};
    std::vector<Point3D> points{};

    [[nodiscard]] std::size_t size() const noexcept { return points.size(); }
    [[nodiscard]] bool empty() const noexcept { return points.empty(); }
};

}  // namespace gpr3d

