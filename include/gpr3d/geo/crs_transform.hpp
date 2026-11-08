#pragma once

#include "gpr3d/core/types.hpp"

#include <optional>

namespace gpr3d::geo {

/// WGS84 ellipsoid parameters (identically practical to ETRS89 / GRS80 at sub-mm scale).
struct Wgs84Ellipsoid {
    static constexpr double a = 6378137.0;                    ///< Semi-major axis (m)
    static constexpr double inv_f = 298.257223563;            ///< Inverse flattening
    static constexpr double f = 1.0 / inv_f;                  ///< Flattening
    static constexpr double b = a * (1.0 - f);                ///< Semi-minor axis (m)
    static constexpr double e2 = f * (2.0 - f);               ///< First eccentricity squared
    static constexpr double ep2 = e2 / (1.0 - e2);            ///< Second eccentricity squared
    static constexpr double n = f / (2.0 - f);                ///< Third flattening
    static constexpr double utm_k0 = 0.9996;                  ///< UTM central meridian scale factor
};

/// Compute standard UTM zone [1..60] including Norway (32V) and Svalbard (X-band) rules.
[[nodiscard]] int compute_utm_zone(double latitude_deg, double longitude_deg) noexcept;

/// Convert WGS84 geodetic coordinates to UTM projected coordinates (6th-order Krüger series).
/// @param forced_zone Optional fixed UTM zone to maintain continuity across zone boundaries.
[[nodiscard]] UtmCoord wgs84_to_utm(
    const Wgs84Coord& wgs84,
    std::optional<int> forced_zone = std::nullopt) noexcept;

/// Convert UTM projected coordinates back to WGS84 geodetic coordinates.
[[nodiscard]] Wgs84Coord utm_to_wgs84(const UtmCoord& utm) noexcept;

/// Convert WGS84 geodetic coordinates to Earth-Centered Earth-Fixed (ECEF) Cartesian (m).
[[nodiscard]] EcefCoord wgs84_to_ecef(const Wgs84Coord& wgs84) noexcept;

/// Convert ECEF Cartesian coordinates (m) to WGS84 geodetic coordinates.
[[nodiscard]] Wgs84Coord ecef_to_wgs84(const EcefCoord& ecef) noexcept;

/// Convert a WGS84 point into local tangent-plane East-North-Up (ENU) meters relative to origin.
[[nodiscard]] EnuCoord wgs84_to_enu(const Wgs84Coord& point, const Wgs84Coord& origin) noexcept;

/// Convert local tangent-plane East-North-Up (ENU) meters back to WGS84 given origin.
[[nodiscard]] Wgs84Coord enu_to_wgs84(const EnuCoord& enu, const Wgs84Coord& origin) noexcept;

/// Horizontal ellipsoidal distance in meters between two WGS84 coordinates.
[[nodiscard]] double horizontal_distance_m(const Wgs84Coord& a, const Wgs84Coord& b) noexcept;

/// Forward azimuth (heading) in degrees [0, 360) clockwise from true North from point a to point b.
[[nodiscard]] double forward_azimuth_deg(const Wgs84Coord& a, const Wgs84Coord& b) noexcept;

/// Apply GNSS-to-radar antenna lever-arm offset in vehicle frame (forward, right, up in meters).
[[nodiscard]] Wgs84Coord apply_lever_arm(
    const Wgs84Coord& gnss_pos,
    double heading_deg,
    double forward_m,
    double right_m,
    double up_m) noexcept;

}  // namespace gpr3d::geo

