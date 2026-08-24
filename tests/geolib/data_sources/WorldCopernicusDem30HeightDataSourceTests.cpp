#include "geolib/data_sources/WorldCopernicusDem30HeightDataSource.h"

#include "TestSupport.h"

#include <memory>
#include <string>
#include <vector>

using namespace geo;

namespace {

using TileKey = WorldCopernicusDem30HeightDataSource::TileKey;

/// Builds a 2x2 tile with a constant height covering the whole degree square.
std::shared_ptr<GridHeightDataSource> makeTile(const TileKey& key, double height)
{
    return std::make_shared<GridHeightDataSource>("test tile",
                                                  WorldCopernicusDem30HeightDataSource::boundsFor(key),
                                                  2, 2, std::vector<double>(4, height), 30.0);
}

void testTileKeyNaming()
{
    CHECK_EQ_STR(TileKey{48, 11}.toString(), "N48_00_E011_00");
    CHECK_EQ_STR(TileKey{-34, -59}.toString(), "S34_00_W059_00");
    CHECK_EQ_STR(TileKey{0, 0}.toString(), "N00_00_E000_00");
}

void testTileKeyFor()
{
    const auto key = WorldCopernicusDem30HeightDataSource::tileKeyFor(48.137, 11.575);
    CHECK_EQ_INT(key.latDeg, 48);
    CHECK_EQ_INT(key.lonDeg, 11);

    // Negative coordinates round towards the south west corner.
    const auto south = WorldCopernicusDem30HeightDataSource::tileKeyFor(-33.45, -70.66);
    CHECK_EQ_INT(south.latDeg, -34);
    CHECK_EQ_INT(south.lonDeg, -71);
}

void testCoverageIsGlobal()
{
    const WorldCopernicusDem30HeightDataSource source({});
    CHECK_TRUE(source.covers(48.0, 11.0));
    CHECK_TRUE(source.covers(-45.0, 170.0));
    CHECK_FALSE(source.covers(95.0, 11.0));
    CHECK_NEAR(source.resolutionM(), 30.0, 1e-9);
    CHECK_FALSE(source.name().empty());
}

void testSamplingUsesTheMatchingTile()
{
    WorldCopernicusDem30HeightDataSource source([](const TileKey& key) {
        return makeTile(key, key.latDeg == 48 && key.lonDeg == 11 ? 500.0 : 100.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(48.5, 11.5, height));
    CHECK_NEAR(height, 500.0, 1e-9);
    CHECK_TRUE(source.sampleHeight(49.5, 11.5, height));
    CHECK_NEAR(height, 100.0, 1e-9);
}

/// Tiles must only be requested once, including tiles that are unavailable.
void testTilesAreCached()
{
    int loads = 0;
    WorldCopernicusDem30HeightDataSource source([&](const TileKey& key) {
        ++loads;
        return makeTile(key, 42.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(48.2, 11.2, height));
    CHECK_TRUE(source.sampleHeight(48.8, 11.8, height));
    CHECK_EQ_INT(loads, 1);

    int missingLoads = 0;
    WorldCopernicusDem30HeightDataSource ocean([&](const TileKey&) {
        ++missingLoads;
        return nullptr;
    });
    CHECK_FALSE(ocean.sampleHeight(10.5, -30.5, height));
    CHECK_FALSE(ocean.sampleHeight(10.5, -30.5, height));
    // One request for the tile itself, the border logic adds no neighbours here.
    CHECK_EQ_INT(missingLoads, 1);
}

void testMissingTileFails()
{
    WorldCopernicusDem30HeightDataSource source([](const TileKey&) { return nullptr; });
    double height = 123.0;
    CHECK_FALSE(source.sampleHeight(48.5, 11.5, height));
}

void testWithoutLoader()
{
    const WorldCopernicusDem30HeightDataSource source({});
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(48.5, 11.5, height));
}

/// A location exactly on a tile border must be served by the neighbour if the
/// containing tile is unavailable.
void testBorderFallsBackToNeighbour()
{
    WorldCopernicusDem30HeightDataSource source([](const TileKey& key) {
        if (key.latDeg == 48 && key.lonDeg == 11) {
            return std::shared_ptr<GridHeightDataSource>();
        }
        return makeTile(key, 77.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(48.0, 11.0, height));
    CHECK_NEAR(height, 77.0, 1e-9);
}

void testBoundsFor()
{
    const auto bounds = WorldCopernicusDem30HeightDataSource::boundsFor(TileKey{48, 11});
    CHECK_NEAR(bounds.minLatitudeDeg, 48.0, 1e-9);
    CHECK_NEAR(bounds.maxLatitudeDeg, 49.0, 1e-9);
    CHECK_NEAR(bounds.minLongitudeDeg, 11.0, 1e-9);
    CHECK_NEAR(bounds.maxLongitudeDeg, 12.0, 1e-9);
}

} // namespace

int main()
{
    testTileKeyNaming();
    testTileKeyFor();
    testCoverageIsGlobal();
    testSamplingUsesTheMatchingTile();
    testTilesAreCached();
    testMissingTileFails();
    testWithoutLoader();
    testBorderFallsBackToNeighbour();
    testBoundsFor();
    return geotest::summarize("WorldCopernicusDem30HeightDataSourceTests");
}
