#include "geolib/data_sources/GermanyDgm5HeightDataSource.h"

#include "geolib/UtmProjection.h"

#include <cmath>
#include <string>
#include <utility>

namespace geo {
namespace {

/// Grid spacing of the DGM5 in metres; also the width of the border strip in
/// which the interpolation stencil reaches into the neighbouring tile.
constexpr double kSpacingM = 5.0;
constexpr double kTileSizeM = 1000.0;

} // namespace

std::string GermanyDgm5HeightDataSource::TileKey::toString() const
{
    return std::to_string(eastKm) + "_" + std::to_string(northKm);
}

GermanyDgm5HeightDataSource::GermanyDgm5HeightDataSource(TileLoader loader)
    : m_loader(std::move(loader))
{
}

void GermanyDgm5HeightDataSource::toUtm32(double latitudeDeg, double longitudeDeg, double& eastingM,
                                      double& northingM)
{
    Utm32Projection::forward(latitudeDeg, longitudeDeg, eastingM, northingM);
}

GermanyDgm5HeightDataSource::TileKey GermanyDgm5HeightDataSource::tileKeyFor(double latitudeDeg,
                                                                    double longitudeDeg)
{
    double easting = 0.0;
    double northing = 0.0;
    Utm32Projection::forward(latitudeDeg, longitudeDeg, easting, northing);
    TileKey key;
    key.eastKm = static_cast<int>(std::floor(easting / kTileSizeM));
    key.northKm = static_cast<int>(std::floor(northing / kTileSizeM));
    return key;
}

std::shared_ptr<Utm32GridTile> GermanyDgm5HeightDataSource::tile(const TileKey& key) const
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

bool GermanyDgm5HeightDataSource::sampleHeight(double latitudeDeg, double longitudeDeg,
                                           double& heightM) const
{
    if (!covers(latitudeDeg, longitudeDeg)) {
        return false;
    }

    double easting = 0.0;
    double northing = 0.0;
    Utm32Projection::forward(latitudeDeg, longitudeDeg, easting, northing);

    TileKey key;
    key.eastKm = static_cast<int>(std::floor(easting / kTileSizeM));
    key.northKm = static_cast<int>(std::floor(northing / kTileSizeM));

    if (const auto raster = tile(key)) {
        if (raster->sampleUtm(easting, northing, heightM)) {
            return true;
        }
    }

    // Close to a tile border the interpolation stencil reaches into the
    // neighbouring tile; try the adjacent tiles before giving up.
    const double localEast = easting - key.eastKm * kTileSizeM;
    const double localNorth = northing - key.northKm * kTileSizeM;
    const int eastStep =
        (localEast < kSpacingM) ? -1 : ((localEast > kTileSizeM - kSpacingM) ? 1 : 0);
    const int northStep =
        (localNorth < kSpacingM) ? -1 : ((localNorth > kTileSizeM - kSpacingM) ? 1 : 0);

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
