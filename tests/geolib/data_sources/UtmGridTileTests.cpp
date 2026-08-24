#include "geolib/data_sources/UtmGridTile.h"

#include "TestSupport.h"

#include "geolib/UtmProjection.h"

#include <vector>

using namespace geo;

namespace {

/// 3x3 tile in UTM zone 16N, height = 100 + column + 2 * row.
UtmGridTile makeTile()
{
    std::vector<double> heights;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            heights.push_back(100.0 + column + 2.0 * row);
        }
    }
    return UtmGridTile(16, 540000.0, 4400000.0, 1.0, 3, 3, std::move(heights));
}

void testValidity()
{
    CHECK_FALSE(UtmGridTile().valid());
    CHECK_TRUE(makeTile().valid());
    CHECK_FALSE(UtmGridTile(16, 0.0, 0.0, 1.0, 1, 1, {1.0}).valid());
}

void testAccessors()
{
    const UtmGridTile tile = makeTile();
    CHECK_EQ_INT(tile.zone(), 16);
    CHECK_NEAR(tile.originEasting(), 540000.0, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 4400000.0, 1e-9);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-9);
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
}

void testSampleAtNodes()
{
    const UtmGridTile tile = makeTile();
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(540000.0, 4400000.0, height));
    CHECK_NEAR(height, 100.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(540002.0, 4400000.0, height));
    CHECK_NEAR(height, 102.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(540000.0, 4400002.0, height));
    CHECK_NEAR(height, 104.0, 1e-9);
}

void testBilinearInterpolation()
{
    const UtmGridTile tile = makeTile();
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(540000.5, 4400000.0, height));
    CHECK_NEAR(height, 100.5, 1e-9);
    CHECK_TRUE(tile.sampleUtm(540000.0, 4400000.5, height));
    CHECK_NEAR(height, 101.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(540000.5, 4400000.5, height));
    CHECK_NEAR(height, 101.5, 1e-9);
}

void testOutOfBounds()
{
    const UtmGridTile tile = makeTile();
    double height = 0.0;
    CHECK_FALSE(tile.sampleUtm(539999.0, 4400000.0, height));
    CHECK_FALSE(tile.sampleUtm(540003.0, 4400000.0, height));
    CHECK_FALSE(tile.sampleUtm(540000.0, 4399999.0, height));
    CHECK_FALSE(tile.sampleUtm(540000.0, 4400003.0, height));
    CHECK_TRUE(tile.sampleUtm(540002.0, 4400002.0, height));
}

void testNoDataIsRejected()
{
    std::vector<double> heights{1.0, 2.0, 3.0, -9999.0};
    const UtmGridTile tile(16, 0.0, 0.0, 1.0, 2, 2, std::move(heights));
    double height = 0.0;
    CHECK_FALSE(tile.sampleUtm(0.5, 0.5, height));
}

/// sampleGeodetic must project into the zone stored in the tile.
void testSampleGeodeticUsesTheTileZone()
{
    constexpr double kLat = 39.7392;   // Denver, UTM zone 13N
    constexpr double kLon = -104.9903;
    const int zone = UtmProjection::zoneForLongitude(kLon);
    CHECK_EQ_INT(zone, 13);

    double easting = 0.0;
    double northing = 0.0;
    UtmProjection(zone).forward(kLat, kLon, easting, northing);

    std::vector<double> heights;
    for (int row = 0; row < 11; ++row) {
        for (int column = 0; column < 11; ++column) {
            heights.push_back(1600.0 + 0.25 * column + 0.5 * row);
        }
    }
    const UtmGridTile tile(zone, easting - 5.0, northing - 5.0, 1.0, 11, 11, std::move(heights));

    double viaGeodetic = 0.0;
    double viaUtm = 0.0;
    CHECK_TRUE(tile.sampleGeodetic(kLat, kLon, viaGeodetic));
    CHECK_TRUE(tile.sampleUtm(easting, northing, viaUtm));
    CHECK_NEAR(viaGeodetic, viaUtm, 1e-9);

    // The same raster placed in a different zone does not contain the point.
    const UtmGridTile wrongZone(32, tile.originEasting(), tile.originNorthing(), 1.0, 11, 11,
                                std::vector<double>(121, 1600.0));
    double height = 0.0;
    CHECK_FALSE(wrongZone.sampleGeodetic(kLat, kLon, height));
}

} // namespace

int main()
{
    testValidity();
    testAccessors();
    testSampleAtNodes();
    testBilinearInterpolation();
    testOutOfBounds();
    testNoDataIsRejected();
    testSampleGeodeticUsesTheTileZone();
    return geotest::summarize("UtmGridTileTests");
}
