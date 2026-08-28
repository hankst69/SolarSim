#pragma once

#include <string>

namespace geo {

/// Transverse Mercator projection of the British National Grid (OSGB36,
/// EPSG:27700), the grid all Ordnance Survey and Environment Agency data
/// products are published on.
///
/// Unlike UTM this grid uses the Airy 1830 ellipsoid and the OSGB36 datum, so
/// geodetic WGS84 coordinates additionally have to pass a Helmert datum shift.
/// Both steps are done here, i.e. the interface takes and returns WGS84
/// latitude/longitude just like UtmProjection.
class BritishNationalGridProjection {
public:
    /// Geodetic WGS84 latitude/longitude in degrees -> BNG easting/northing in
    /// metres.
    static void forward(double latitudeDeg, double longitudeDeg, double& eastingM,
                        double& northingM);

    /// BNG easting/northing in metres -> geodetic WGS84 latitude/longitude in
    /// degrees.
    static void inverse(double eastingM, double northingM, double& latitudeDeg,
                        double& longitudeDeg);

    /// Two letter code of the 100 km square containing the position, e.g.
    /// "SU". Returns an empty string outside the lettered area.
    static std::string squareFor(double eastingM, double northingM);

    /// South west corner of a 100 km square. Returns false for unknown codes.
    static bool squareOrigin(const std::string& square, double& eastingM, double& northingM);
};

} // namespace geo
