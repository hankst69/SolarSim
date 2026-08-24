#include "geolib/BavariaDgm1HeightDataSource.h"

#include <cmath>
#include <string>
#include <utility>

namespace geo {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg2Rad = kPi / 180.0;

// WGS84 / ETRS89 ellipsoid parameters used by EPSG:25832.
constexpr double kA = 6378137.0;
constexpr double kF = 1.0 / 298.257223563;
constexpr double kK0 = 0.9996;
constexpr double kFalseEasting = 500000.0;
constexpr int kUtmZone32CentralMeridianDeg = 9;

} // namespace

std::string BavariaDgm1HeightDataSource::TileKey::toString() const
{
    return std::to_string(eastKm) + "_" + std::to_string(northKm);
}

BavariaDgm1HeightDataSource::BavariaDgm1HeightDataSource(TileLoader loader)
    : m_loader(std::move(loader))
{
}

void BavariaDgm1HeightDataSource::toUtm32(double latitudeDeg, double longitudeDeg,
                                          double& eastingM, double& northingM)
{
    const double e2 = kF * (2.0 - kF);
    const double ep2 = e2 / (1.0 - e2);

    const double lat = latitudeDeg * kDeg2Rad;
    const double dLon = (longitudeDeg - kUtmZone32CentralMeridianDeg) * kDeg2Rad;

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

BavariaDgm1HeightDataSource::TileKey BavariaDgm1HeightDataSource::tileKeyFor(double latitudeDeg,
                                                                            double longitudeDeg)
{
    double easting = 0.0;
    double northing = 0.0;
    toUtm32(latitudeDeg, longitudeDeg, easting, northing);
    TileKey key;
    key.eastKm = static_cast<int>(std::floor(easting / 1000.0));
    key.northKm = static_cast<int>(std::floor(northing / 1000.0));
    return key;
}

std::shared_ptr<GridHeightDataSource> BavariaDgm1HeightDataSource::tile(const TileKey& key) const
{
    const auto it = m_tiles.find(key);
    if (it != m_tiles.end()) {
        return it->second;
    }
    std::shared_ptr<GridHeightDataSource> loaded;
    if (m_loader) {
        loaded = m_loader(key);
    }
    m_tiles.emplace(key, loaded);
    return loaded;
}

bool BavariaDgm1HeightDataSource::sampleHeight(double latitudeDeg, double longitudeDeg,
                                               double& heightM) const
{
    if (!covers(latitudeDeg, longitudeDeg)) {
        return false;
    }
    const auto raster = tile(tileKeyFor(latitudeDeg, longitudeDeg));
    if (!raster) {
        return false;
    }
    return raster->sampleHeight(latitudeDeg, longitudeDeg, heightM);
}

} // namespace geo
