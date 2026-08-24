#include "geolib/UtmProjection.h"

#include "TestSupport.h"

using namespace geo;

namespace {

/// Zone 32N must reproduce the reference coordinates of the EPSG:25832 grid.
void testZone32ReferencePoints()
{
    double easting = 0.0;
    double northing = 0.0;

    // Munich, Marienplatz area.
    Utm32Projection::forward(48.1372, 11.5756, easting, northing);
    CHECK_NEAR(easting, 691611.22, 0.02);
    CHECK_NEAR(northing, 5334758.05, 0.02);

    // A point on the central meridian gets the false easting exactly.
    Utm32Projection::forward(50.0, 9.0, easting, northing);
    CHECK_NEAR(easting, 500000.0, 1e-6);
}

void testCentralMeridian()
{
    CHECK_NEAR(Utm32Projection::projection().centralMeridianDeg(), 9.0, 1e-12);
    CHECK_EQ_INT(Utm32Projection::projection().zone(), 32);

    CHECK_NEAR(UtmProjection(31).centralMeridianDeg(), 3.0, 1e-12);
    CHECK_NEAR(UtmProjection(13).centralMeridianDeg(), -105.0, 1e-12);
    CHECK_NEAR(UtmProjection(1).centralMeridianDeg(), -177.0, 1e-12);
    CHECK_NEAR(UtmProjection(60).centralMeridianDeg(), 177.0, 1e-12);
}

void testRoundTrip()
{
    struct Sample {
        int zone;
        double latitudeDeg;
        double longitudeDeg;
    };

    const Sample samples[] = {
        {32, 48.1372, 11.5756},   // Munich
        {32, 47.2000, 10.0000},   // southern edge of Bavaria
        {32, 50.6000, 12.5000},   // northern edge of Bavaria
        {31, 48.8566, 2.3522},    // Paris
        {13, 39.7392, -104.9903}, // Denver
        {33, 59.9139, 10.7522},   // Oslo
    };

    for (const auto& sample : samples) {
        const UtmProjection projection(sample.zone);
        double easting = 0.0;
        double northing = 0.0;
        projection.forward(sample.latitudeDeg, sample.longitudeDeg, easting, northing);

        double latitude = 0.0;
        double longitude = 0.0;
        projection.inverse(easting, northing, latitude, longitude);

        CHECK_NEAR(latitude, sample.latitudeDeg, 1e-7);
        CHECK_NEAR(longitude, sample.longitudeDeg, 1e-7);

        // Northern hemisphere UTM keeps easting inside the standard band.
        CHECK_TRUE(easting > 100000.0 && easting < 900000.0);
        CHECK_TRUE(northing > 0.0 && northing < 10000000.0);
    }
}

/// One metre east must stay one metre east after projecting (scale ~ 1).
void testLocalScale()
{
    double e0 = 0.0;
    double n0 = 0.0;
    double e1 = 0.0;
    double n1 = 0.0;

    Utm32Projection::forward(48.1372, 11.5756, e0, n0);
    Utm32Projection::inverse(e0 + 100.0, n0, e1, n1); // e1/n1 hold lat/lon here

    double eBack = 0.0;
    double nBack = 0.0;
    Utm32Projection::forward(e1, n1, eBack, nBack);
    // The inverse is accurate to ~1e-9 degrees, i.e. well below a millimetre.
    CHECK_NEAR(eBack - e0, 100.0, 1e-3);
    CHECK_NEAR(nBack - n0, 0.0, 1e-3);
}

void testZoneForLongitude()
{
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(11.5756), 32);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(2.3522), 31);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(-104.9903), 13);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(10.7522), 32);

    // Zone boundaries and the extremes of the range. Zone 1 spans -180..-174,
    // and +180 wraps onto -180.
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(-180.0), 1);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(-174.1), 1);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(-173.9), 2);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(0.0), 31);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(179.9), 60);
    CHECK_EQ_INT(UtmProjection::zoneForLongitude(180.0), 1);
}

} // namespace

int main()
{
    testZone32ReferencePoints();
    testCentralMeridian();
    testRoundTrip();
    testLocalScale();
    testZoneForLongitude();
    return geotest::summarize("UtmProjectionTests");
}
