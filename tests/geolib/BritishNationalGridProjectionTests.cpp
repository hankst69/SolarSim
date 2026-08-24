#include "geolib/BritishNationalGridProjection.h"

#include "TestSupport.h"

#include <string>

using namespace geo;

namespace {

/// Reference points of the OSGB36 National Grid (WGS84 input).
void testReferencePoints()
{
    double easting = 0.0;
    double northing = 0.0;

    // Official OS example (Caister water tower), the Helmert datum shift used
    // here is accurate to a few metres.
    BritishNationalGridProjection::forward(52.0 + 39.0 / 60.0 + 28.8282 / 3600.0,
                                           1.0 + 42.0 / 60.0 + 57.8663 / 3600.0, easting,
                                           northing);
    CHECK_NEAR(easting, 651409.903, 5.0);
    CHECK_NEAR(northing, 313177.270, 5.0);

    // Edinburgh Castle -> NT 251 735.
    BritishNationalGridProjection::forward(55.9486, -3.1999, easting, northing);
    CHECK_NEAR(easting, 325160.0, 10.0);
    CHECK_NEAR(northing, 673490.0, 10.0);
}

void testRoundTrip()
{
    struct Sample {
        double latitudeDeg;
        double longitudeDeg;
    };

    const Sample samples[] = {
        {51.5080, -0.1281},  // London
        {53.4808, -2.2426},  // Manchester
        {50.3755, -4.1427},  // Plymouth
        {54.9783, -1.6178},  // Newcastle
        {51.4816, -3.1791},  // Cardiff
    };

    for (const auto& sample : samples) {
        double easting = 0.0;
        double northing = 0.0;
        BritishNationalGridProjection::forward(sample.latitudeDeg, sample.longitudeDeg, easting,
                                               northing);

        double latitude = 0.0;
        double longitude = 0.0;
        BritishNationalGridProjection::inverse(easting, northing, latitude, longitude);
        CHECK_NEAR(latitude, sample.latitudeDeg, 1e-6);
        CHECK_NEAR(longitude, sample.longitudeDeg, 1e-6);
    }
}

void testSquareCodes()
{
    // Origins of some well known 100 km squares.
    CHECK_EQ_STR(BritishNationalGridProjection::squareFor(400000.0, 200000.0), "SP");
    CHECK_EQ_STR(BritishNationalGridProjection::squareFor(530034.0, 180381.0), "TQ");
    CHECK_EQ_STR(BritishNationalGridProjection::squareFor(0.0, 0.0), "SV");
    CHECK_EQ_STR(BritishNationalGridProjection::squareFor(325161.0, 673589.0), "NT");

    double easting = 0.0;
    double northing = 0.0;
    CHECK_TRUE(BritishNationalGridProjection::squareOrigin("SP", easting, northing));
    CHECK_NEAR(easting, 400000.0, 1e-9);
    CHECK_NEAR(northing, 200000.0, 1e-9);

    CHECK_TRUE(BritishNationalGridProjection::squareOrigin("TQ", easting, northing));
    CHECK_NEAR(easting, 500000.0, 1e-9);
    CHECK_NEAR(northing, 100000.0, 1e-9);

    CHECK_TRUE(BritishNationalGridProjection::squareOrigin("NT", easting, northing));
    CHECK_NEAR(easting, 300000.0, 1e-9);
    CHECK_NEAR(northing, 600000.0, 1e-9);
}

/// Every square code must map back to the coordinates it was derived from.
void testSquareRoundTrip()
{
    for (int east = 0; east <= 600000; east += 100000) {
        for (int north = 0; north <= 1200000; north += 100000) {
            const std::string square = BritishNationalGridProjection::squareFor(east, north);
            if (square.empty()) {
                continue;
            }
            double originEast = 0.0;
            double originNorth = 0.0;
            CHECK_TRUE(
                BritishNationalGridProjection::squareOrigin(square, originEast, originNorth));
            CHECK_NEAR(originEast, east, 1e-9);
            CHECK_NEAR(originNorth, north, 1e-9);
        }
    }
}

void testInvalidSquares()
{
    CHECK_TRUE(BritishNationalGridProjection::squareFor(-1.0, 100.0).empty());
    CHECK_TRUE(BritishNationalGridProjection::squareFor(100.0, -1.0).empty());
    CHECK_TRUE(BritishNationalGridProjection::squareFor(900000.0, 100.0).empty());

    double easting = 0.0;
    double northing = 0.0;
    CHECK_FALSE(BritishNationalGridProjection::squareOrigin("S", easting, northing));
    CHECK_FALSE(BritishNationalGridProjection::squareOrigin("SPX", easting, northing));
    CHECK_FALSE(BritishNationalGridProjection::squareOrigin("AA", easting, northing));
    CHECK_FALSE(BritishNationalGridProjection::squareOrigin("I1", easting, northing));
}

} // namespace

int main()
{
    testReferencePoints();
    testRoundTrip();
    testSquareCodes();
    testSquareRoundTrip();
    testInvalidSquares();
    return geotest::summarize("BritishNationalGridProjectionTests");
}
