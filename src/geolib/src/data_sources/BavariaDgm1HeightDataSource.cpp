#include "geolib/data_sources/BavariaDgm1HeightDataSource.h"

#include "geolib/UtmProjection.h"

#include <cmath>
#include <string>
#include <utility>

namespace geo {

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
    Utm32Projection::forward(latitudeDeg, longitudeDeg, eastingM, northingM);
}

BavariaDgm1HeightDataSource::TileKey BavariaDgm1HeightDataSource::tileKeyFor(double latitudeDeg,
                                                                            double longitudeDeg)
{
    double easting = 0.0;
    double northing = 0.0;
    Utm32Projection::forward(latitudeDeg, longitudeDeg, easting, northing);
    TileKey key;
    key.eastKm = static_cast<int>(std::floor(easting / 1000.0));
    key.northKm = static_cast<int>(std::floor(northing / 1000.0));
    return key;
}

std::shared_ptr<Utm32GridTile> BavariaDgm1HeightDataSource::tile(const TileKey& key) const
{
    const auto it = m_tiles.find(key);
    if (it != m_tiles.end()) {
        return it->second;
    }
    std::shared_ptr<Utm32GridTile> loaded;
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

    double easting = 0.0;
    double northing = 0.0;
    Utm32Projection::forward(latitudeDeg, longitudeDeg, easting, northing);

    TileKey key;
    key.eastKm = static_cast<int>(std::floor(easting / 1000.0));
    key.northKm = static_cast<int>(std::floor(northing / 1000.0));

    if (const auto raster = tile(key)) {
        if (raster->sampleUtm(easting, northing, heightM)) {
            return true;
        }
    }

    // Close to a tile border the interpolation stencil reaches into the
    // neighbouring tile; try the adjacent tiles before giving up.
    const double localEast = easting - key.eastKm * 1000.0;
    const double localNorth = northing - key.northKm * 1000.0;
    const int eastStep = (localEast < 1.0) ? -1 : ((localEast > 999.0) ? 1 : 0);
    const int northStep = (localNorth < 1.0) ? -1 : ((localNorth > 999.0) ? 1 : 0);

    for (int de = -1; de <= 1; ++de) {
        for (int dn = -1; dn <= 1; ++dn) {
            if ((de == 0 && dn == 0) || (de != 0 && de != eastStep) ||
                (dn != 0 && dn != northStep)) {
                continue;
            }
            const TileKey neighbour{key.eastKm + de, key.northKm + dn};
            if (const auto raster = tile(neighbour)) {
                if (raster->sampleUtm(easting, northing, heightM)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace geo
