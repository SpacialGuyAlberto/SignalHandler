#include "gpr3d/geo/crs_transform.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace gpr3d::geo {

namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kRadToDeg = 180.0 / std::numbers::pi;

struct KruegerConstants {
    double A{};
    std::array<double, 6> alpha{};
    std::array<double, 6> beta{};
};

constexpr KruegerConstants make_krueger_constants() noexcept {
    constexpr double a = Wgs84Ellipsoid::a;
    constexpr double n = Wgs84Ellipsoid::n;
    const double n2 = n * n;
    const double n3 = n2 * n;
    const double n4 = n3 * n;
    const double n5 = n4 * n;
    const double n6 = n5 * n;

    KruegerConstants c{};
    c.A = (a / (1.0 + n)) * (1.0 + n2 / 4.0 + n4 / 64.0 + n6 / 256.0);

    c.alpha = {
        (1.0 / 2.0) * n - (2.0 / 3.0) * n2 + (5.0 / 16.0) * n3 + (41.0 / 180.0) * n4 -
            (127.0 / 288.0) * n5 + (7891.0 / 37800.0) * n6,
        (13.0 / 48.0) * n2 - (3.0 / 5.0) * n3 + (557.0 / 1440.0) * n4 + (281.0 / 630.0) * n5 -
            (1983433.0 / 1935360.0) * n6,
        (61.0 / 240.0) * n3 - (103.0 / 140.0) * n4 + (15061.0 / 26880.0) * n5 +
            (167603.0 / 181440.0) * n6,
        (49561.0 / 161280.0) * n4 - (179.0 / 168.0) * n5 + (6601661.0 / 7257600.0) * n6,
        (34729.0 / 80640.0) * n5 - (3418889.0 / 1995840.0) * n6,
        (212378941.0 / 319334400.0) * n6
    };

    c.beta = {
        (1.0 / 2.0) * n - (2.0 / 3.0) * n2 + (37.0 / 96.0) * n3 - (1.0 / 360.0) * n4 -
            (81.0 / 512.0) * n5 + (96199.0 / 604800.0) * n6,
        (1.0 / 48.0) * n2 + (1.0 / 15.0) * n3 - (437.0 / 1440.0) * n4 + (46.0 / 105.0) * n5 -
            (1118711.0 / 3870720.0) * n6,
        (17.0 / 480.0) * n3 - (37.0 / 840.0) * n4 - (209.0 / 4480.0) * n5 +
            (5569.0 / 90720.0) * n6,
        (4397.0 / 161280.0) * n4 - (11.0 / 504.0) * n5 - (830251.0 / 7257600.0) * n6,
        (4583.0 / 161280.0) * n5 - (108847.0 / 3991680.0) * n6,
        (20648693.0 / 638668800.0) * n6
    };

    return c;
}

constexpr KruegerConstants kKrueger = make_krueger_constants();

double normalize_lon_deg(double lon_deg) noexcept {
    double lon = std::fmod(lon_deg + 180.0, 360.0);
    if (lon < 0.0) {
        lon += 360.0;
    }
    return lon - 180.0;
}

double tau_prime_from_tau(double tau) noexcept {
    const double e = std::sqrt(Wgs84Ellipsoid::e2);
    const double sigma = std::sinh(e * std::atanh((e * tau) / std::hypot(1.0, tau)));
    return tau * std::hypot(1.0, sigma) - sigma * std::hypot(1.0, tau);
}

}  // namespace

int compute_utm_zone(double latitude_deg, double longitude_deg) noexcept {
    const double lon = normalize_lon_deg(longitude_deg);
    int zone = static_cast<int>(std::floor((lon + 180.0) / 6.0)) + 1;
    zone = std::clamp(zone, 1, 60);

    // Special Norway zone 32V exception
    if (latitude_deg >= 56.0 && latitude_deg < 64.0 && lon >= 3.0 && lon < 12.0) {
        return 32;
    }

    // Special Svalbard X-band exceptions
    if (latitude_deg >= 72.0 && latitude_deg < 84.0) {
        if (lon >= 0.0 && lon < 9.0) return 31;
        if (lon >= 9.0 && lon < 21.0) return 33;
        if (lon >= 21.0 && lon < 33.0) return 35;
        if (lon >= 33.0 && lon < 42.0) return 37;
    }

    return zone;
}

UtmCoord wgs84_to_utm(const Wgs84Coord& wgs84, std::optional<int> forced_zone) noexcept {
    const double lat_deg = std::clamp(wgs84.latitude_deg, -84.0, 84.0);
    const double lon_deg = normalize_lon_deg(wgs84.longitude_deg);
    const int zone = forced_zone.has_value()
                         ? std::clamp(*forced_zone, 1, 60)
                         : compute_utm_zone(lat_deg, lon_deg);
    const bool is_northern = (lat_deg >= 0.0);

    const double lon0_deg = static_cast<double>(zone * 6 - 183);
    const double dlon_rad = normalize_lon_deg(lon_deg - lon0_deg) * kDegToRad;
    const double lat_rad = lat_deg * kDegToRad;

    const double tau = std::tan(lat_rad);
    const double tau_p = tau_prime_from_tau(tau);

    const double cos_dlon = std::cos(dlon_rad);
    const double sin_dlon = std::sin(dlon_rad);

    const double xi_p = std::atan2(tau_p, cos_dlon);
    const double eta_p = std::asinh(sin_dlon / std::hypot(tau_p, cos_dlon));

    double xi = xi_p;
    double eta = eta_p;
    for (int j = 1; j <= 6; ++j) {
        const double two_j = 2.0 * static_cast<double>(j);
        const double alpha_j = kKrueger.alpha[static_cast<std::size_t>(j - 1)];
        xi += alpha_j * std::sin(two_j * xi_p) * std::cosh(two_j * eta_p);
        eta += alpha_j * std::cos(two_j * xi_p) * std::sinh(two_j * eta_p);
    }

    const double k0_A = Wgs84Ellipsoid::utm_k0 * kKrueger.A;
    const double easting = 500000.0 + k0_A * eta;
    const double northing = (is_northern ? 0.0 : 10000000.0) + k0_A * xi;

    return UtmCoord{
        .easting_m = easting,
        .northing_m = northing,
        .elevation_m = wgs84.altitude_m,
        .zone = zone,
        .is_northern = is_northern
    };
}

Wgs84Coord utm_to_wgs84(const UtmCoord& utm) noexcept {
    const double k0_A = Wgs84Ellipsoid::utm_k0 * kKrueger.A;
    const double false_northing = utm.is_northern ? 0.0 : 10000000.0;

    const double xi = (utm.northing_m - false_northing) / k0_A;
    const double eta = (utm.easting_m - 500000.0) / k0_A;

    double xi_p = xi;
    double eta_p = eta;
    for (int j = 1; j <= 6; ++j) {
        const double two_j = 2.0 * static_cast<double>(j);
        const double beta_j = kKrueger.beta[static_cast<std::size_t>(j - 1)];
        xi_p -= beta_j * std::sin(two_j * xi) * std::cosh(two_j * eta);
        eta_p -= beta_j * std::cos(two_j * xi) * std::sinh(two_j * eta);
    }

    const double sinh_eta_p = std::sinh(eta_p);
    const double cos_xi_p = std::cos(xi_p);
    const double sin_xi_p = std::sin(xi_p);

    const double tau_p0 = sin_xi_p / std::hypot(sinh_eta_p, cos_xi_p);

    // Karney quadratic Newton iteration from conformal tangent tau' to geodetic tangent tau
    double tau = tau_p0;
    constexpr double one_minus_e2 = 1.0 - Wgs84Ellipsoid::e2;
    for (int iter = 0; iter < 6; ++iter) {
        const double tau_pi = tau_prime_from_tau(tau);
        const double dtau = ((tau_p0 - tau_pi) / std::hypot(1.0, tau_pi)) *
                            ((1.0 + one_minus_e2 * tau * tau) / (one_minus_e2 * std::hypot(1.0, tau)));
        tau += dtau;
        if (std::abs(dtau) <= 1e-15 * std::max(1.0, std::abs(tau))) {
            break;
        }
    }

    const double lat_deg = std::atan(tau) * kRadToDeg;
    const double lon0_deg = static_cast<double>(utm.zone * 6 - 183);
    const double dlon_deg = std::atan2(sinh_eta_p, cos_xi_p) * kRadToDeg;

    return Wgs84Coord{
        .latitude_deg = lat_deg,
        .longitude_deg = normalize_lon_deg(lon0_deg + dlon_deg),
        .altitude_m = utm.elevation_m
    };
}

EcefCoord wgs84_to_ecef(const Wgs84Coord& wgs84) noexcept {
    const double lat = wgs84.latitude_deg * kDegToRad;
    const double lon = wgs84.longitude_deg * kDegToRad;
    const double sin_lat = std::sin(lat);
    const double cos_lat = std::cos(lat);
    const double sin_lon = std::sin(lon);
    const double cos_lon = std::cos(lon);

    const double N = Wgs84Ellipsoid::a / std::sqrt(1.0 - Wgs84Ellipsoid::e2 * sin_lat * sin_lat);
    return EcefCoord{
        .x_m = (N + wgs84.altitude_m) * cos_lat * cos_lon,
        .y_m = (N + wgs84.altitude_m) * cos_lat * sin_lon,
        .z_m = (N * (1.0 - Wgs84Ellipsoid::e2) + wgs84.altitude_m) * sin_lat
    };
}

Wgs84Coord ecef_to_wgs84(const EcefCoord& ecef) noexcept {
    const double p = std::hypot(ecef.x_m, ecef.y_m);
    const double lon = std::atan2(ecef.y_m, ecef.x_m);

    // Bowring initial estimate + 3 exact geodetic iterations
    const double theta = std::atan2(ecef.z_m * Wgs84Ellipsoid::a, p * Wgs84Ellipsoid::b);
    const double sin_t = std::sin(theta);
    const double cos_t = std::cos(theta);
    double lat = std::atan2(
        ecef.z_m + Wgs84Ellipsoid::ep2 * Wgs84Ellipsoid::b * sin_t * sin_t * sin_t,
        p - Wgs84Ellipsoid::e2 * Wgs84Ellipsoid::a * cos_t * cos_t * cos_t);

    double alt = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double sin_lat = std::sin(lat);
        const double cos_lat = std::cos(lat);
        const double N = Wgs84Ellipsoid::a / std::sqrt(1.0 - Wgs84Ellipsoid::e2 * sin_lat * sin_lat);
        if (std::abs(cos_lat) > 1e-10) {
            alt = p / cos_lat - N;
        } else {
            alt = std::abs(ecef.z_m) - Wgs84Ellipsoid::b;
        }
        lat = std::atan2(ecef.z_m, p * (1.0 - Wgs84Ellipsoid::e2 * (N / (N + alt))));
    }

    return Wgs84Coord{
        .latitude_deg = lat * kRadToDeg,
        .longitude_deg = normalize_lon_deg(lon * kRadToDeg),
        .altitude_m = alt
    };
}

EnuCoord wgs84_to_enu(const Wgs84Coord& point, const Wgs84Coord& origin) noexcept {
    const EcefCoord p_ecef = wgs84_to_ecef(point);
    const EcefCoord o_ecef = wgs84_to_ecef(origin);

    const double dx = p_ecef.x_m - o_ecef.x_m;
    const double dy = p_ecef.y_m - o_ecef.y_m;
    const double dz = p_ecef.z_m - o_ecef.z_m;

    const double lat0 = origin.latitude_deg * kDegToRad;
    const double lon0 = origin.longitude_deg * kDegToRad;
    const double sin_lat = std::sin(lat0);
    const double cos_lat = std::cos(lat0);
    const double sin_lon = std::sin(lon0);
    const double cos_lon = std::cos(lon0);

    return EnuCoord{
        .east_m = -sin_lon * dx + cos_lon * dy,
        .north_m = -sin_lat * cos_lon * dx - sin_lat * sin_lon * dy + cos_lat * dz,
        .up_m = cos_lat * cos_lon * dx + cos_lat * sin_lon * dy + sin_lat * dz
    };
}

Wgs84Coord enu_to_wgs84(const EnuCoord& enu, const Wgs84Coord& origin) noexcept {
    const EcefCoord o_ecef = wgs84_to_ecef(origin);

    const double lat0 = origin.latitude_deg * kDegToRad;
    const double lon0 = origin.longitude_deg * kDegToRad;
    const double sin_lat = std::sin(lat0);
    const double cos_lat = std::cos(lat0);
    const double sin_lon = std::sin(lon0);
    const double cos_lon = std::cos(lon0);

    const double dx = -sin_lon * enu.east_m - sin_lat * cos_lon * enu.north_m + cos_lat * cos_lon * enu.up_m;
    const double dy =  cos_lon * enu.east_m - sin_lat * sin_lon * enu.north_m + cos_lat * sin_lon * enu.up_m;
    const double dz =                         cos_lat * enu.north_m +           sin_lat * enu.up_m;

    return ecef_to_wgs84(EcefCoord{
        .x_m = o_ecef.x_m + dx,
        .y_m = o_ecef.y_m + dy,
        .z_m = o_ecef.z_m + dz
    });
}

double horizontal_distance_m(const Wgs84Coord& a, const Wgs84Coord& b) noexcept {
    Wgs84Coord b_level = b;
    b_level.altitude_m = a.altitude_m;
    const EnuCoord enu = wgs84_to_enu(b_level, a);
    return std::hypot(enu.east_m, enu.north_m);
}

double forward_azimuth_deg(const Wgs84Coord& a, const Wgs84Coord& b) noexcept {
    const EnuCoord enu = wgs84_to_enu(b, a);
    if (std::hypot(enu.east_m, enu.north_m) < 1e-12) {
        return 0.0;
    }
    double az = std::atan2(enu.east_m, enu.north_m) * kRadToDeg;
    if (az < 0.0) {
        az += 360.0;
    }
    return az;
}

Wgs84Coord apply_lever_arm(
    const Wgs84Coord& gnss_pos,
    double heading_deg,
    double forward_m,
    double right_m,
    double up_m) noexcept {
    if (forward_m == 0.0 && right_m == 0.0 && up_m == 0.0) {
        return gnss_pos;
    }
    const double psi = heading_deg * kDegToRad;
    const double sin_psi = std::sin(psi);
    const double cos_psi = std::cos(psi);

    // Vehicle frame (Forward along heading, Right = heading + 90 deg, Up) -> Local ENU
    const EnuCoord offset_enu{
        .east_m = forward_m * sin_psi + right_m * cos_psi,
        .north_m = forward_m * cos_psi - right_m * sin_psi,
        .up_m = up_m
    };
    return enu_to_wgs84(offset_enu, gnss_pos);
}

}  // namespace gpr3d::geo

