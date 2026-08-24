#include "geolib/data_sources/UsaUsgs3Dep1mHeightDataSource.h"

#include "geolib/UtmProjection.h"

#include <cmath>
#include <string>
#include <utility>

namespace geo {

std::string UsaUsgs3Dep1mHeightDataSource::TileKey::toString() const
{
    if (zone < 1 || zone > 60) {
        return std::string();
    }
    // The official names give the south west corner in full kilometres.
    return std::to_string(zone) + "_x" + std::to_string(eastingM / 1000) + "y" +
           std::to_string(northingM / 1000);
}

UsaUsgs3Dep1mHeightDataSource::UsaUsgs3Dep1mHeightDataSource(TileLoader loader)
    : m_loader(std::move(loader))
{
}

void UsaUsgs3Dep1mHeightDataSource::toUtm(double latitudeDeg, double longitudeDeg, int& zone,
                                          double& eastingM, double& northingM)
{
    zone = UtmProjection::zoneForLongitude(longitudeDeg);
    UtmProjection(zone).forward(latitudeDeg, longitudeDeg, eastingM, northingM);
}

UsaUsgs3Dep1mHeightDataSource::TileKey
UsaUsgs3Dep1mHeightDataSource::tileKeyFor(double latitudeDeg, double longitudeDeg)
{
    int zone = 0;
    double easting = 0.0;
    double northing = 0.0;
    toUtm(latitudeDeg, longitudeDeg, zone, easting, northing);

    const auto step = static_cast<int>(kTileSizeM);
    TileKey key;
    key.zone = zone;
    key.eastingM = static_cast<int>(std::floor(easting / kTileSizeM)) * step;
    key.northingM = static_cast<int>(std::floor(northing / kTileSizeM)) * step;
    return key;
}

std::shared_ptr<UtmGridTile> UsaUsgs3Dep1mHeightDataSource::tile(const TileKey& key) const
{
    const auto it = m_tiles.find(key);
    if (it != m_tiles.end()) {
        return it->second;
    }
    std::shared_ptr<UtmGridTile> loaded;
    if (m_loader) {
        loaded = m_loader(key);
    }
    m_tiles.emplace(key, loaded);
    return loaded;
}

bool UsaUsgs3Dep1mHeightDataSource::sampleHeight(double latitudeDeg, double longitudeDeg,
                                                 double& heightM) const
{
    if (!covers(latitudeDeg, longitudeDeg)) {
        return false;
    }

    int zone = 0;
    double easting = 0.0;
    double northing = 0.0;
    toUtm(latitudeDeg, longitudeDeg, zone, easting, northing);

    const auto step = static_cast<int>(kTileSizeM);
    TileKey key;
    key.zone = zone;
    key.eastingM = static_cast<int>(std::floor(easting / kTileSizeM)) * step;
    key.northingM = static_cast<int>(std::floor(northing / kTileSizeM)) * step;

    if (const auto raster = tile(key)) {
        if (raster->sampleGeodetic(latitudeDeg, longitudeDeg, heightM)) {
            return true;
        }
    }

    // Close to a tile border the interpolation stencil reaches into the
    // neighbouring tile; try the adjacent tiles before giving up. Tiles of the
    // neighbouring UTM zone are not considered - they use a different grid and
    // are looked up through their own tile key.
    const double localEast = easting - key.eastingM;
    const double localNorth = northing - key.northingM;
    const int eastStep = (localEast < 1.0) ? -1 : ((localEast > kTileSizeM - 1.0) ? 1 : 0);
    const int northStep = (localNorth < 1.0) ? -1 : ((localNorth > kTileSizeM - 1.0) ? 1 : 0);

    for (int de = -1; de <= 1; ++de) {
        for (int dn = -1; dn <= 1; ++dn) {
            if ((de == 0 && dn == 0) || (de != 0 && de != eastStep) ||
                (dn != 0 && dn != northStep)) {
                continue;
            }
            const TileKey neighbour{key.zone, key.eastingM + de * step,
                                    key.northingM + dn * step};
            if (const auto raster = tile(neighbour)) {
                if (raster->sampleGeodetic(latitudeDeg, longitudeDeg, heightM)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace geo
