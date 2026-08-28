#include "geolib/data_sources/UkEaLidarHeightDataSource.h"

#include "geolib/BritishNationalGridProjection.h"

#include <cmath>
#include <string>
#include <utility>

namespace geo {

std::string UkEaLidarHeightDataSource::TileKey::toString() const
{
    const std::string square =
        BritishNationalGridProjection::squareFor(eastingM, northingM);
    if (square.empty()) {
        return std::string();
    }

    double squareEast = 0.0;
    double squareNorth = 0.0;
    if (!BritishNationalGridProjection::squareOrigin(square, squareEast, squareNorth)) {
        return std::string();
    }

    const int localEast = eastingM - static_cast<int>(squareEast);
    const int localNorth = northingM - static_cast<int>(squareNorth);

    // 10 km cell inside the 100 km square...
    const int cellEast = localEast / 10000;
    const int cellNorth = localNorth / 10000;
    // ...and the quadrant of the 5 km block inside that cell.
    const bool east = (localEast % 10000) >= 5000;
    const bool north = (localNorth % 10000) >= 5000;

    std::string name;
    name.reserve(6);
    for (const char c : square) {
        name.push_back(static_cast<char>(c + ('a' - 'A')));
    }
    name.push_back(static_cast<char>('0' + cellEast));
    name.push_back(static_cast<char>('0' + cellNorth));
    name.append(north ? "n" : "s");
    name.append(east ? "e" : "w");
    return name;
}

UkEaLidarHeightDataSource::UkEaLidarHeightDataSource(TileLoader loader)
    : m_loader(std::move(loader))
{
}

void UkEaLidarHeightDataSource::toBng(double latitudeDeg, double longitudeDeg, double& eastingM,
                                      double& northingM)
{
    BritishNationalGridProjection::forward(latitudeDeg, longitudeDeg, eastingM, northingM);
}

UkEaLidarHeightDataSource::TileKey UkEaLidarHeightDataSource::tileKeyFor(double latitudeDeg,
                                                                        double longitudeDeg)
{
    double easting = 0.0;
    double northing = 0.0;
    BritishNationalGridProjection::forward(latitudeDeg, longitudeDeg, easting, northing);
    TileKey key;
    key.eastingM = static_cast<int>(std::floor(easting / kTileSizeM) * kTileSizeM);
    key.northingM = static_cast<int>(std::floor(northing / kTileSizeM) * kTileSizeM);
    return key;
}

std::shared_ptr<BngGridTile> UkEaLidarHeightDataSource::tile(const TileKey& key) const
{
    const auto it = m_tiles.find(key);
    if (it != m_tiles.end()) {
        return it->second;
    }
    std::shared_ptr<BngGridTile> loaded;
    if (m_loader) {
        loaded = m_loader(key);
    }
    m_tiles.emplace(key, loaded);
    return loaded;
}

bool UkEaLidarHeightDataSource::sampleHeight(double latitudeDeg, double longitudeDeg,
                                             double& heightM) const
{
    if (!covers(latitudeDeg, longitudeDeg)) {
        return false;
    }

    double easting = 0.0;
    double northing = 0.0;
    BritishNationalGridProjection::forward(latitudeDeg, longitudeDeg, easting, northing);

    const auto step = static_cast<int>(kTileSizeM);
    TileKey key;
    key.eastingM = static_cast<int>(std::floor(easting / kTileSizeM)) * step;
    key.northingM = static_cast<int>(std::floor(northing / kTileSizeM)) * step;

    if (const auto raster = tile(key)) {
        if (raster->sampleBng(easting, northing, heightM)) {
            return true;
        }
    }

    // Close to a tile border the interpolation stencil reaches into the
    // neighbouring tile; try the adjacent tiles before giving up.
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
            const TileKey neighbour{key.eastingM + de * step, key.northingM + dn * step};
            if (const auto raster = tile(neighbour)) {
                if (raster->sampleBng(easting, northing, heightM)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace geo
