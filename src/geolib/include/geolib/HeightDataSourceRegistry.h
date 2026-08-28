#pragma once

#include "geolib/HeightDataSource.h"

#include <vector>

namespace geo {

/// Registry of available height data sources with a selection mechanism based
/// on the geographic location of a GroundPlane.
///
/// Sources are ranked by coverage first and by resolution second, so the most
/// detailed data set covering the standpoint wins. A world wide fallback source
/// (e.g. FlatHeightDataSource or a global DEM) can be registered as well; it is
/// only chosen if no better source covers the location.
///
/// Suggested data sets to register (each needs its own reader/downloader):
///  * Germany / Bavaria: "openData Digitales Gelaendemodell 1m (DGM1)" of the
///    Landesamt fuer Digitalisierung, Breitband und Vermessung (LDBV), 1 m grid.
///  * Germany (nation wide): BKG DGM5 / DGM10, 5 m resp. 10 m grid.
///  * Austria: data.gv.at ALS DGM 1 m; Switzerland: swissALTI3D 0.5 m.
///  * France: IGN RGE ALTI 1 m; Netherlands: AHN 0.5 m; UK: EA LIDAR 1 m.
///  * Nordics: Lantmaeteriet (SE), Kartverket (NO), NLS (FI) 1-2 m LiDAR.
///  * USA: USGS 3DEP 1 m / 10 m; Canada: HRDEM 1-2 m.
///  * Europe wide: EU-DEM 25 m (Copernicus Land Monitoring Service).
///  * World wide: Copernicus DEM GLO-30 (30 m), NASADEM / SRTM (30 m),
///    ALOS AW3D30 (30 m), ASTER GDEM (30 m).
class HeightDataSourceRegistry {
public:
    /// Registry pre-filled with the built-in sources (currently the flat
    /// fallback). Additional sources can be added by the application.
    static HeightDataSourceRegistry& instance();

    void addSource(HeightDataSourcePtr source);
    void clear();

    const std::vector<HeightDataSourcePtr>& sources() const { return m_sources; }

    /// All registered sources covering the given location, best (finest
    /// resolution) first.
    std::vector<HeightDataSourcePtr> sourcesFor(double latitudeDeg,
                                                double longitudeDeg) const;

    /// Best source for the given location, or nullptr if none covers it.
    HeightDataSourcePtr selectSource(double latitudeDeg, double longitudeDeg) const;
    HeightDataSourcePtr selectSource(const GeoLocation& location) const;

private:
    std::vector<HeightDataSourcePtr> m_sources;
};

} // namespace geo
