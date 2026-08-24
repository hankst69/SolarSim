#include "geolib/data_sources/BngGridTile.h"

#include "geolib/BritishNationalGridProjection.h"
#include "TestSupport.h"

#include <vector>

using namespace geo;

namespace {

/// 3x3 tile with a plane rising 1 m per metre towards the east.
BngGridTile makeRamp()
{
    std::vector<double> heights;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            heights.push_back(static_cast<double>(column));
        }
    }
    return BngGridTile(400000.0, 200000.0, 1.0, 3, 3, std::move(heights));
}

void testValidity()
{
    const BngGridTile empty;
    CHECK_FALSE(empty.valid());

    const auto tile = makeRamp();
    CHECK_TRUE(tile.valid());
    CHECK_EQ_INT(tile.columns(), 3);
    CHECK_EQ_INT(tile.rows(), 3);
    CHECK_NEAR(tile.cellSize(), 1.0, 1e-12);
    CHECK_NEAR(tile.originEasting(), 400000.0, 1e-9);
    CHECK_NEAR(tile.originNorthing(), 200000.0, 1e-9);
}

void testBilinearInterpolation()
{
    const auto tile = makeRamp();
    double height = 0.0;

    CHECK_TRUE(tile.sampleBng(400000.0, 200000.0, height));
    CHECK_NEAR(height, 0.0, 1e-12);
    CHECK_TRUE(tile.sampleBng(400002.0, 200002.0, height));
    CHECK_NEAR(height, 2.0, 1e-12);
    CHECK_TRUE(tile.sampleBng(400000.5, 200001.5, height));
    CHECK_NEAR(height, 0.5, 1e-12);
}

void testOutsideTile()
{
    const auto tile = makeRamp();
    double height = 0.0;
    CHECK_FALSE(tile.sampleBng(399999.0, 200000.0, height));
    CHECK_FALSE(tile.sampleBng(400000.0, 199999.0, height));
    CHECK_FALSE(tile.sampleBng(400003.0, 200000.0, height));
    CHECK_FALSE(tile.sampleBng(400000.0, 200003.0, height));
}

void testNoDataIsRejected()
{
    std::vector<double> heights(9, 10.0);
    heights[4] = -9999.0; // centre cell
    const BngGridTile tile(400000.0, 200000.0, 1.0, 3, 3, std::move(heights));

    double height = 0.0;
    CHECK_FALSE(tile.sampleBng(400001.0, 200001.0, height));
    // A stencil that avoids the gap still works.
    CHECK_TRUE(tile.sampleBng(400002.0, 200002.0, height));
    CHECK_NEAR(height, 10.0, 1e-12);
}

/// A geodetic query must be projected to the National Grid first.
void testGeodeticSampling()
{
    double easting = 0.0;
    double northing = 0.0;
    BritishNationalGridProjection::forward(51.5, -1.0, easting, northing);

    const BngGridTile tile(easting - 1.0, northing - 1.0, 1.0, 3, 3, std::vector<double>(9, 42.0));
    double height = 0.0;
    CHECK_TRUE(tile.sampleGeodetic(51.5, -1.0, height));
    CHECK_NEAR(height, 42.0, 1e-12);

    // A location far away is outside the tile.
    CHECK_FALSE(tile.sampleGeodetic(52.5, -1.0, height));
}

} // namespace

int main()
{
    testValidity();
    testBilinearInterpolation();
    testOutsideTile();
    testNoDataIsRejected();
    testGeodeticSampling();
    return geotest::summarize("BngGridTileTests");
}
