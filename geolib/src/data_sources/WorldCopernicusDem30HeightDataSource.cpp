#include "geolib/data_sources/WorldCopernicusDem30HeightDataSource.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace geo {
namespace {

/// Zero padded degree label as used in the official tile names, e.g. "N48_00".
std::string degreeLabel(int value, char positive, char negative, int width)
{
    std::ostringstream os;
    os << (value < 0 ? negative : positive) << std::setw(width) << std::setfill('0')
       << std::abs(value) << "_00";
    return os.str();
}

} // namespace

std::string WorldCopernicusDem30HeightDataSource::TileKey::toString() const
{
    return degreeLabel(latDeg, 'N', 'S', 2) + "_" + degreeLabel(lonDeg, 'E', 'W', 3);
}

WorldCopernicusDem30HeightDataSource::WorldCopernicusDem30HeightDataSource(TileLoader loader)
    : m_loader(std::move(loader))
{
}

WorldCopernicusDem30HeightDataSource::TileKey
WorldCopernicusDem30HeightDataSource::tileKeyFor(double latitudeDeg, double longitudeDeg)
{
    TileKey key;
    key.latDeg = static_cast<int>(std::floor(latitudeDeg));
    key.lonDeg = static_cast<int>(std::floor(longitudeDeg));
    return key;
}

GeoBounds WorldCopernicusDem30HeightDataSource::boundsFor(const TileKey& key)
{
    return {static_cast<double>(key.latDeg), static_cast<double>(key.latDeg) + 1.0,
            static_cast<double>(key.lonDeg), static_cast<double>(key.lonDeg) + 1.0};
}

std::shared_ptr<GridHeightDataSource>
WorldCopernicusDem30HeightDataSource::tile(const TileKey& key) const
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

bool WorldCopernicusDem30HeightDataSource::sampleHeight(double latitudeDeg, double longitudeDeg,
                                                   double& heightM) const
{
    if (!covers(latitudeDeg, longitudeDeg)) {
        return false;
    }

    const TileKey key = tileKeyFor(latitudeDeg, longitudeDeg);
    if (const auto raster = tile(key)) {
        if (raster->sampleHeight(latitudeDeg, longitudeDeg, heightM)) {
            return true;
        }
    }

    // Exactly on a tile border the sample also belongs to the neighbouring
    // tile; try the adjacent tiles before giving up.
    const double localLat = latitudeDeg - key.latDeg;
    const double localLon = longitudeDeg - key.lonDeg;
    const int latStep = (localLat <= 0.0) ? -1 : ((localLat >= 1.0) ? 1 : 0);
    const int lonStep = (localLon <= 0.0) ? -1 : ((localLon >= 1.0) ? 1 : 0);

    for (int dlat = -1; dlat <= 1; ++dlat) {
        for (int dlon = -1; dlon <= 1; ++dlon) {
            if ((dlat == 0 && dlon == 0) || (dlat != 0 && dlat != latStep) ||
                (dlon != 0 && dlon != lonStep)) {
                continue;
            }
            const TileKey neighbour{key.latDeg + dlat, key.lonDeg + dlon};
            if (const auto raster = tile(neighbour)) {
                if (raster->sampleHeight(latitudeDeg, longitudeDeg, heightM)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace geo
