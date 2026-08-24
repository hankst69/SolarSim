#pragma once

namespace geo {

/// Transverse Mercator (UTM) projection on the WGS84/ETRS89 ellipsoid.
///
/// The zone only enters through its central meridian (6 * zone - 183 degrees),
/// so a single implementation serves all zones. Zone 32N (EPSG:25832) is used
/// by the German, Austrian and Danish open data elevation models; other data
/// sets use neighbouring zones (France 31, Nordics 32-35, USA 10-19).
class UtmProjection {
public:
    /// Northern hemisphere UTM zone (1..60).
    explicit UtmProjection(int zone = 32) : m_zone(zone) {}

    int zone() const { return m_zone; }

    /// Central meridian of the zone in degrees.
    double centralMeridianDeg() const { return 6.0 * m_zone - 183.0; }

    /// Geodetic latitude/longitude in degrees -> UTM easting/northing in metres.
    void forward(double latitudeDeg, double longitudeDeg, double& eastingM,
                 double& northingM) const;

    /// UTM easting/northing in metres -> geodetic latitude/longitude in degrees.
    void inverse(double eastingM, double northingM, double& latitudeDeg,
                 double& longitudeDeg) const;

    /// UTM zone containing the given longitude.
    static int zoneForLongitude(double longitudeDeg);

private:
    int m_zone{32};
};

/// Convenience accessor for UTM zone 32N (EPSG:25832), the projection of the
/// German open data elevation models.
class Utm32Projection {
public:
    static const UtmProjection& projection();

    static void forward(double latitudeDeg, double longitudeDeg, double& eastingM,
                        double& northingM)
    {
        projection().forward(latitudeDeg, longitudeDeg, eastingM, northingM);
    }

    static void inverse(double eastingM, double northingM, double& latitudeDeg,
                        double& longitudeDeg)
    {
        projection().inverse(eastingM, northingM, latitudeDeg, longitudeDeg);
    }
};

} // namespace geo
