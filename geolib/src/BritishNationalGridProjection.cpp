#include "geolib/BritishNationalGridProjection.h"

#include <cctype>
#include <cmath>
#include <string>

namespace geo {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 180.0 / kPi;

// Airy 1830 ellipsoid and the parameters of the National Grid projection.
constexpr double kAiryA = 6377563.396;
constexpr double kAiryB = 6356256.909;
constexpr double kF0 = 0.9996012717;
constexpr double kLat0 = 49.0 * kDeg2Rad;
constexpr double kLon0 = -2.0 * kDeg2Rad;
constexpr double kE0 = 400000.0;
constexpr double kN0 = -100000.0;

// WGS84 ellipsoid.
constexpr double kWgsA = 6378137.0;
constexpr double kWgsF = 1.0 / 298.257223563;

/// Helmert transformation WGS84 -> OSGB36 (OS "Transformation and projections"
/// guide, accurate to a few metres, which is far below the 1 m raster spacing
/// of the LIDAR tiles).
constexpr double kTx = -446.448;
constexpr double kTy = 125.157;
constexpr double kTz = -542.060;
constexpr double kScale = 20.4894e-6;
constexpr double kRx = -0.1502 / 3600.0 * kDeg2Rad;
constexpr double kRy = -0.2470 / 3600.0 * kDeg2Rad;
constexpr double kRz = -0.8421 / 3600.0 * kDeg2Rad;

struct Cartesian {
    double x{0.0};
    double y{0.0};
    double z{0.0};
};

Cartesian toCartesian(double latRad, double lonRad, double a, double b)
{
    const double e2 = (a * a - b * b) / (a * a);
    const double sinLat = std::sin(latRad);
    const double cosLat = std::cos(latRad);
    const double nu = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
    return {nu * cosLat * std::cos(lonRad), nu * cosLat * std::sin(lonRad),
            (1.0 - e2) * nu * sinLat};
}

void fromCartesian(const Cartesian& p, double a, double b, double& latRad, double& lonRad)
{
    const double e2 = (a * a - b * b) / (a * a);
    const double planar = std::sqrt(p.x * p.x + p.y * p.y);
    double lat = std::atan2(p.z, planar * (1.0 - e2));
    // Converges within a few iterations for terrestrial coordinates.
    for (int i = 0; i < 10; ++i) {
        const double sinLat = std::sin(lat);
        const double nu = a / std::sqrt(1.0 - e2 * sinLat * sinLat);
        const double next = std::atan2(p.z + e2 * nu * sinLat, planar);
        if (std::fabs(next - lat) < 1e-14) {
            lat = next;
            break;
        }
        lat = next;
    }
    latRad = lat;
    lonRad = std::atan2(p.y, p.x);
}

Cartesian helmert(const Cartesian& p, double tx, double ty, double tz, double rx, double ry,
                  double rz, double scale)
{
    const double s = 1.0 + scale;
    return {tx + p.x * s - p.y * rz + p.z * ry, ty + p.x * rz + p.y * s - p.z * rx,
            tz - p.x * ry + p.y * rx + p.z * s};
}

/// Meridional arc of the National Grid projection.
double meridionalArc(double latRad)
{
    const double n = (kAiryA - kAiryB) / (kAiryA + kAiryB);
    const double n2 = n * n;
    const double n3 = n2 * n;
    const double dLat = latRad - kLat0;
    const double sLat = latRad + kLat0;
    return kAiryB * kF0 *
           ((1.0 + n + 1.25 * n2 + 1.25 * n3) * dLat -
            (3.0 * n + 3.0 * n2 + 2.625 * n3) * std::sin(dLat) * std::cos(sLat) +
            (1.875 * n2 + 1.875 * n3) * std::sin(2.0 * dLat) * std::cos(2.0 * sLat) -
            (35.0 / 24.0 * n3) * std::sin(3.0 * dLat) * std::cos(3.0 * sLat));
}

/// Grid letters in their National Grid order; 'I' is not used.
const char* kLetters = "ABCDEFGHJKLMNOPQRSTUVWXYZ";

} // namespace

void BritishNationalGridProjection::forward(double latitudeDeg, double longitudeDeg,
                                            double& eastingM, double& northingM)
{
    const double wgsB = kWgsA * (1.0 - kWgsF);
    const Cartesian wgs = toCartesian(latitudeDeg * kDeg2Rad, longitudeDeg * kDeg2Rad, kWgsA, wgsB);
    const Cartesian osgb = helmert(wgs, kTx, kTy, kTz, kRx, kRy, kRz, kScale);

    double lat = 0.0;
    double lon = 0.0;
    fromCartesian(osgb, kAiryA, kAiryB, lat, lon);

    const double e2 = (kAiryA * kAiryA - kAiryB * kAiryB) / (kAiryA * kAiryA);
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double tanLat = std::tan(lat);

    const double nu = kAiryA * kF0 / std::sqrt(1.0 - e2 * sinLat * sinLat);
    const double rho = kAiryA * kF0 * (1.0 - e2) / std::pow(1.0 - e2 * sinLat * sinLat, 1.5);
    const double eta2 = nu / rho - 1.0;

    const double m = meridionalArc(lat);
    const double t2 = tanLat * tanLat;
    const double t4 = t2 * t2;

    const double i = m + kN0;
    const double ii = nu / 2.0 * sinLat * cosLat;
    const double iii = nu / 24.0 * sinLat * cosLat * cosLat * cosLat * (5.0 - t2 + 9.0 * eta2);
    const double iiiA =
        nu / 720.0 * sinLat * std::pow(cosLat, 5) * (61.0 - 58.0 * t2 + t4);
    const double iv = nu * cosLat;
    const double v = nu / 6.0 * cosLat * cosLat * cosLat * (nu / rho - t2);
    const double vi =
        nu / 120.0 * std::pow(cosLat, 5) * (5.0 - 18.0 * t2 + t4 + 14.0 * eta2 - 58.0 * t2 * eta2);

    const double dLon = lon - kLon0;
    const double dLon2 = dLon * dLon;

    northingM = i + ii * dLon2 + iii * dLon2 * dLon2 + iiiA * dLon2 * dLon2 * dLon2;
    eastingM = kE0 + iv * dLon + v * dLon2 * dLon + vi * dLon2 * dLon2 * dLon;
}

void BritishNationalGridProjection::inverse(double eastingM, double northingM, double& latitudeDeg,
                                            double& longitudeDeg)
{
    const double e2 = (kAiryA * kAiryA - kAiryB * kAiryB) / (kAiryA * kAiryA);

    double lat = kLat0;
    double m = 0.0;
    // Iterate the latitude until the meridional arc matches the northing.
    for (int i = 0; i < 20; ++i) {
        lat += (northingM - kN0 - m) / (kAiryA * kF0);
        m = meridionalArc(lat);
        if (std::fabs(northingM - kN0 - m) < 1e-5) {
            break;
        }
    }

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double tanLat = std::tan(lat);

    const double nu = kAiryA * kF0 / std::sqrt(1.0 - e2 * sinLat * sinLat);
    const double rho = kAiryA * kF0 * (1.0 - e2) / std::pow(1.0 - e2 * sinLat * sinLat, 1.5);
    const double eta2 = nu / rho - 1.0;

    const double t2 = tanLat * tanLat;
    const double t4 = t2 * t2;
    const double t6 = t4 * t2;

    const double vii = tanLat / (2.0 * rho * nu);
    const double viii = tanLat / (24.0 * rho * nu * nu * nu) * (5.0 + 3.0 * t2 + eta2 - 9.0 * t2 * eta2);
    const double ix = tanLat / (720.0 * rho * std::pow(nu, 5)) * (61.0 + 90.0 * t2 + 45.0 * t4);
    const double x = 1.0 / (cosLat * nu);
    const double xi = 1.0 / (cosLat * 6.0 * nu * nu * nu) * (nu / rho + 2.0 * t2);
    const double xii = 1.0 / (cosLat * 120.0 * std::pow(nu, 5)) * (5.0 + 28.0 * t2 + 24.0 * t4);
    const double xiiA =
        1.0 / (cosLat * 5040.0 * std::pow(nu, 7)) * (61.0 + 662.0 * t2 + 1320.0 * t4 + 720.0 * t6);

    const double dE = eastingM - kE0;
    const double dE2 = dE * dE;

    const double osgbLat = lat - vii * dE2 + viii * dE2 * dE2 - ix * dE2 * dE2 * dE2;
    const double osgbLon =
        kLon0 + x * dE - xi * dE2 * dE + xii * dE2 * dE2 * dE - xiiA * dE2 * dE2 * dE2 * dE;

    // Datum shift back to WGS84 (inverse Helmert).
    const Cartesian osgb = toCartesian(osgbLat, osgbLon, kAiryA, kAiryB);
    const Cartesian wgs =
        helmert(osgb, -kTx, -kTy, -kTz, -kRx, -kRy, -kRz, -kScale);

    const double wgsB = kWgsA * (1.0 - kWgsF);
    double latRad = 0.0;
    double lonRad = 0.0;
    fromCartesian(wgs, kWgsA, wgsB, latRad, lonRad);
    latitudeDeg = latRad * kRad2Deg;
    longitudeDeg = lonRad * kRad2Deg;
}

std::string BritishNationalGridProjection::squareFor(double eastingM, double northingM)
{
    if (eastingM < 0.0 || northingM < 0.0) {
        return std::string();
    }
    const int east100k = static_cast<int>(std::floor(eastingM / 100000.0));
    const int north100k = static_cast<int>(std::floor(northingM / 100000.0));
    if (east100k > 6 || north100k > 12) {
        return std::string();
    }

    // Index of the 500 km square, then of the 100 km square inside it.
    const int first = (19 - north100k) - (19 - north100k) % 5 + (east100k + 10) / 5;
    const int second = ((19 - north100k) * 5) % 25 + east100k % 5;
    if (first < 0 || first > 24 || second < 0 || second > 24) {
        return std::string();
    }
    return std::string{kLetters[first], kLetters[second]};
}

bool BritishNationalGridProjection::squareOrigin(const std::string& square, double& eastingM,
                                                 double& northingM)
{
    if (square.size() != 2) {
        return false;
    }
    const auto index = [](char c) {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        for (int i = 0; i < 25; ++i) {
            if (kLetters[i] == upper) {
                return i;
            }
        }
        return -1;
    };
    const int first = index(square[0]);
    const int second = index(square[1]);
    if (first < 0 || second < 0) {
        return false;
    }

    const int east = (((first - 2) % 5 + 5) % 5) * 5 + (second % 5);
    const int north = (19 - (first / 5) * 5) - (second / 5);
    if (east < 0 || east > 6 || north < 0 || north > 12) {
        return false;
    }
    eastingM = east * 100000.0;
    northingM = north * 100000.0;
    return true;
}

} // namespace geo
