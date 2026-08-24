#include "geolib/UtmProjection.h"

#include <algorithm>
#include <cmath>

namespace geo {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 180.0 / kPi;

// WGS84 / ETRS89 ellipsoid parameters (shared by all UTM zones, e.g. EPSG:25832).
constexpr double kA = 6378137.0;
constexpr double kF = 1.0 / 298.257223563;
constexpr double kK0 = 0.9996;
constexpr double kFalseEasting = 500000.0;

} // namespace

int UtmProjection::zoneForLongitude(double longitudeDeg)
{
    double normalized = std::fmod(longitudeDeg + 180.0, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    const int zone = static_cast<int>(normalized / 6.0) + 1;
    return std::min(std::max(zone, 1), 60);
}

void UtmProjection::forward(double latitudeDeg, double longitudeDeg, double& eastingM,
                            double& northingM) const
{
    const double e2 = kF * (2.0 - kF);
    const double ep2 = e2 / (1.0 - e2);

    const double lat = latitudeDeg * kDeg2Rad;
    const double dLon = (longitudeDeg - centralMeridianDeg()) * kDeg2Rad;

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double tanLat = std::tan(lat);

    const double n = kA / std::sqrt(1.0 - e2 * sinLat * sinLat);
    const double t = tanLat * tanLat;
    const double c = ep2 * cosLat * cosLat;
    const double a = cosLat * dLon;

    const double m = kA * ((1.0 - e2 / 4.0 - 3.0 * e2 * e2 / 64.0 -
                            5.0 * e2 * e2 * e2 / 256.0) * lat -
                           (3.0 * e2 / 8.0 + 3.0 * e2 * e2 / 32.0 +
                            45.0 * e2 * e2 * e2 / 1024.0) * std::sin(2.0 * lat) +
                           (15.0 * e2 * e2 / 256.0 + 45.0 * e2 * e2 * e2 / 1024.0) *
                               std::sin(4.0 * lat) -
                           (35.0 * e2 * e2 * e2 / 3072.0) * std::sin(6.0 * lat));

    eastingM = kFalseEasting +
               kK0 * n *
                   (a + (1.0 - t + c) * a * a * a / 6.0 +
                    (5.0 - 18.0 * t + t * t + 72.0 * c - 58.0 * ep2) * a * a * a * a * a / 120.0);

    northingM = kK0 * (m + n * tanLat *
                               (a * a / 2.0 + (5.0 - t + 9.0 * c + 4.0 * c * c) * a * a * a * a / 24.0 +
                                (61.0 - 58.0 * t + t * t + 600.0 * c - 330.0 * ep2) * a * a * a * a * a * a /
                                    720.0));
}

void UtmProjection::inverse(double eastingM, double northingM, double& latitudeDeg,
                            double& longitudeDeg) const
{
    const double e2 = kF * (2.0 - kF);
    const double ep2 = e2 / (1.0 - e2);
    const double e1 = (1.0 - std::sqrt(1.0 - e2)) / (1.0 + std::sqrt(1.0 - e2));

    const double x = eastingM - kFalseEasting;
    const double m = northingM / kK0;
    const double mu = m / (kA * (1.0 - e2 / 4.0 - 3.0 * e2 * e2 / 64.0 -
                                 5.0 * e2 * e2 * e2 / 256.0));

    const double phi1 = mu + (3.0 * e1 / 2.0 - 27.0 * e1 * e1 * e1 / 32.0) * std::sin(2.0 * mu) +
                        (21.0 * e1 * e1 / 16.0 - 55.0 * e1 * e1 * e1 * e1 / 32.0) *
                            std::sin(4.0 * mu) +
                        (151.0 * e1 * e1 * e1 / 96.0) * std::sin(6.0 * mu) +
                        (1097.0 * e1 * e1 * e1 * e1 / 512.0) * std::sin(8.0 * mu);

    const double sinPhi1 = std::sin(phi1);
    const double cosPhi1 = std::cos(phi1);
    const double tanPhi1 = std::tan(phi1);

    const double c1 = ep2 * cosPhi1 * cosPhi1;
    const double t1 = tanPhi1 * tanPhi1;
    const double n1 = kA / std::sqrt(1.0 - e2 * sinPhi1 * sinPhi1);
    const double r1 = kA * (1.0 - e2) / std::pow(1.0 - e2 * sinPhi1 * sinPhi1, 1.5);
    const double d = x / (n1 * kK0);

    const double lat =
        phi1 - (n1 * tanPhi1 / r1) *
                   (d * d / 2.0 -
                    (5.0 + 3.0 * t1 + 10.0 * c1 - 4.0 * c1 * c1 - 9.0 * ep2) * d * d * d * d / 24.0 +
                    (61.0 + 90.0 * t1 + 298.0 * c1 + 45.0 * t1 * t1 - 252.0 * ep2 -
                     3.0 * c1 * c1) *
                        d * d * d * d * d * d / 720.0);

    const double lon =
        (d - (1.0 + 2.0 * t1 + c1) * d * d * d / 6.0 +
         (5.0 - 2.0 * c1 + 28.0 * t1 - 3.0 * c1 * c1 + 8.0 * ep2 + 24.0 * t1 * t1) * d * d * d * d *
             d / 120.0) /
        cosPhi1;

    latitudeDeg = lat * kRad2Deg;
    longitudeDeg = centralMeridianDeg() + lon * kRad2Deg;
}

const UtmProjection& Utm32Projection::projection()
{
    static const UtmProjection instance(32);
    return instance;
}

} // namespace geo
