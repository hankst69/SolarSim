#include "geolib/data_sources/Utm32GridTile.h"

#include "TestSupport.h"

#include "geolib/UtmProjection.h"

#include <vector>

using namespace geo;

namespace {

/// 3x3 tile at 690000/5334000, height = 100 + column + 2 * row.
Utm32GridTile makeTile()
{
    std::vector<double> heights;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            heights.push_back(100.0 + column + 2.0 * row);
        }
    }
    return Utm32GridTile(690000.0, 5334000.0, 1.0, 3, 3, std::move(heights));
}

void testValidity()
{
    CHECK_FALSE(Utm32GridTile().valid());
    CHECK_TRUE(makeTile().valid());

    // A single row/column cannot be interpolated.
    CHECK_FALSE(Utm32GridTile(0.0, 0.0, 1.0, 1, 1, {1.0}).valid());
}

void testAccessors()
{
    const Utm32GridTile tile = makeTile();
    CHECK_NEAR(tile.originEasting(), 690000.0, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 5334000.0, 1e-9);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-9);
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
}

void testSampleAtNodes()
{
    const Utm32GridTile tile = makeTile();
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690000.0, 5334000.0, height));
    CHECK_NEAR(height, 100.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(690002.0, 5334000.0, height));
    CHECK_NEAR(height, 102.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(690000.0, 5334002.0, height));
    CHECK_NEAR(height, 104.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(690002.0, 5334002.0, height));
    CHECK_NEAR(height, 106.0, 1e-9);
}

void testBilinearInterpolation()
{
    const Utm32GridTile tile = makeTile();
    double height = 0.0;

    // Along east: +1 per metre.
    CHECK_TRUE(tile.sampleUtm(690000.5, 5334000.0, height));
    CHECK_NEAR(height, 100.5, 1e-9);
    // Along north: +2 per metre.
    CHECK_TRUE(tile.sampleUtm(690000.0, 5334000.5, height));
    CHECK_NEAR(height, 101.0, 1e-9);
    // Cell centre.
    CHECK_TRUE(tile.sampleUtm(690000.5, 5334000.5, height));
    CHECK_NEAR(height, 101.5, 1e-9);
}

void testOutOfBounds()
{
    const Utm32GridTile tile = makeTile();
    double height = 0.0;
    CHECK_FALSE(tile.sampleUtm(689999.0, 5334000.0, height));
    CHECK_FALSE(tile.sampleUtm(690003.0, 5334000.0, height));
    CHECK_FALSE(tile.sampleUtm(690000.0, 5333999.0, height));
    CHECK_FALSE(tile.sampleUtm(690000.0, 5334003.0, height));

    // The upper edge is still inside.
    CHECK_TRUE(tile.sampleUtm(690002.0, 5334002.0, height));
}

void testNoDataIsRejected()
{
    std::vector<double> heights{1.0, 2.0, 3.0, -9999.0};
    const Utm32GridTile tile(0.0, 0.0, 1.0, 2, 2, std::move(heights));
    double height = 0.0;
    CHECK_FALSE(tile.sampleUtm(0.5, 0.5, height));
    // The no-data corner is part of every stencil in this 2x2 tile.
    CHECK_FALSE(tile.sampleUtm(0.0, 0.0, height));
}

/// sampleGeodetic must agree with an explicit projection followed by sampleUtm.
void testSampleGeodeticMatchesProjection()
{
    // Place a tile around the projected position of a Bavarian coordinate.
    constexpr double kLat = 48.1372;
    constexpr double kLon = 11.5756;
    double easting = 0.0;
    double northing = 0.0;
    Utm32Projection::forward(kLat, kLon, easting, northing);

    const double originEast = easting - 5.0;
    const double originNorth = northing - 5.0;

    std::vector<double> heights;
    for (int row = 0; row < 11; ++row) {
        for (int column = 0; column < 11; ++column) {
            heights.push_back(500.0 + 0.25 * column + 0.5 * row);
        }
    }
    const Utm32GridTile tile(originEast, originNorth, 1.0, 11, 11, std::move(heights));

    double viaGeodetic = 0.0;
    double viaUtm = 0.0;
    CHECK_TRUE(tile.sampleGeodetic(kLat, kLon, viaGeodetic));
    CHECK_TRUE(tile.sampleUtm(easting, northing, viaUtm));
    CHECK_NEAR(viaGeodetic, viaUtm, 1e-9);

    // Centre of the tile: 5 m east and 5 m north of the origin.
    CHECK_NEAR(viaGeodetic, 500.0 + 0.25 * 5.0 + 0.5 * 5.0, 1e-9);
}

void testNonUnitCellSize()
{
    std::vector<double> heights{0.0, 10.0, 20.0, 30.0};
    const Utm32GridTile tile(1000.0, 2000.0, 50.0, 2, 2, std::move(heights));
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(1025.0, 2000.0, height));
    CHECK_NEAR(height, 5.0, 1e-9);
    CHECK_TRUE(tile.sampleUtm(1050.0, 2050.0, height));
    CHECK_NEAR(height, 30.0, 1e-9);
    CHECK_FALSE(tile.sampleUtm(1051.0, 2000.0, height));
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
    testSampleGeodeticMatchesProjection();
    testNonUnitCellSize();
    return geotest::summarize("Utm32GridTileTests");
}
