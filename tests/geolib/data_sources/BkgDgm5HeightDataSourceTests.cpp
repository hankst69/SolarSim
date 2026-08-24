#include "geolib/data_sources/BkgDgm5HeightDataSource.h"

#include "geolib/UtmProjection.h"
#include "TestSupport.h"

#include <memory>
#include <vector>

using namespace geo;

namespace {

using TileKey = BkgDgm5HeightDataSource::TileKey;

/// Tile of constant height covering the whole 1 km square at 5 m spacing.
std::shared_ptr<Utm32GridTile> makeTile(const TileKey& key, double height)
{
    constexpr int kSamples = 201; // 200 * 5 m = 1000 m
    return std::make_shared<Utm32GridTile>(key.eastKm * 1000.0, key.northKm * 1000.0, 5.0, kSamples,
                                           kSamples,
                                           std::vector<double>(kSamples * kSamples, height));
}

void testTileKeyToString()
{
    CHECK_EQ_STR(TileKey{690, 5334}.toString(), "690_5334");
    CHECK_EQ_STR(TileKey{32, 5000}.toString(), "32_5000");
}

void testTileKeyFor()
{
    // Munich, Marienplatz -> UTM32 691611 / 5334758.
    const auto key = BkgDgm5HeightDataSource::tileKeyFor(48.1372, 11.5756);
    CHECK_EQ_INT(key.eastKm, 691);
    CHECK_EQ_INT(key.northKm, 5334);

    double easting = 0.0;
    double northing = 0.0;
    BkgDgm5HeightDataSource::toUtm32(48.1372, 11.5756, easting, northing);
    CHECK_NEAR(easting, 691611.22, 0.02);
    CHECK_NEAR(northing, 5334758.05, 0.02);
}

/// The DGM5 covers all of Germany, not just a single state.
void testCoverage()
{
    const BkgDgm5HeightDataSource source({});
    CHECK_TRUE(source.covers(48.1372, 11.5756)); // Munich
    CHECK_TRUE(source.covers(53.5511, 9.9937));  // Hamburg
    CHECK_TRUE(source.covers(51.0504, 13.7373)); // Dresden
    CHECK_FALSE(source.covers(48.2082, 16.3738)); // Vienna
    CHECK_FALSE(source.covers(51.5074, -0.1278)); // London
    CHECK_NEAR(source.resolutionM(), 5.0, 1e-12);
    CHECK_FALSE(source.name().empty());
}

void testSamplingUsesTheMatchingTile()
{
    BkgDgm5HeightDataSource source([](const TileKey& key) {
        return makeTile(key, key.eastKm == 691 && key.northKm == 5334 ? 519.0 : 100.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(48.1372, 11.5756, height));
    CHECK_NEAR(height, 519.0, 1e-9);
}

/// Tiles must only be requested once, including tiles that are unavailable.
void testTilesAreCached()
{
    int loads = 0;
    BkgDgm5HeightDataSource source([&](const TileKey& key) {
        ++loads;
        return makeTile(key, 300.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(48.1372, 11.5756, height));
    CHECK_TRUE(source.sampleHeight(48.1373, 11.5757, height));
    CHECK_EQ_INT(loads, 1);

    int missing = 0;
    BkgDgm5HeightDataSource gaps([&](const TileKey&) {
        ++missing;
        return nullptr;
    });
    CHECK_FALSE(gaps.sampleHeight(48.1372, 11.5756, height));
    CHECK_FALSE(gaps.sampleHeight(48.1372, 11.5756, height));
    CHECK_EQ_INT(missing, 1);
}

void testOutsideCoverageFails()
{
    BkgDgm5HeightDataSource source([](const TileKey& key) { return makeTile(key, 10.0); });
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(51.5074, -0.1278, height));
}

void testMissingTileFails()
{
    BkgDgm5HeightDataSource source([](const TileKey&) { return nullptr; });
    double height = 123.0;
    CHECK_FALSE(source.sampleHeight(48.1372, 11.5756, height));
}

void testWithoutLoader()
{
    const BkgDgm5HeightDataSource source({});
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(48.1372, 11.5756, height));
}

/// Right on a tile border the stencil reaches into the neighbouring tile, which
/// has to be consulted before the sample is reported as unavailable.
void testBorderFallsBackToNeighbour()
{
    const TileKey border{691, 5334};
    BkgDgm5HeightDataSource source([&](const TileKey& key) {
        if (key.eastKm == border.eastKm && key.northKm == border.northKm) {
            return std::shared_ptr<Utm32GridTile>();
        }
        // The neighbours overlap the border by one grid step.
        return std::make_shared<Utm32GridTile>(key.eastKm * 1000.0 - 5.0,
                                               key.northKm * 1000.0 - 5.0, 5.0, 203, 203,
                                               std::vector<double>(203 * 203, 55.0));
    });

    // A location a few centimetres inside the western edge of the tile.
    double latitude = 0.0;
    double longitude = 0.0;
    Utm32Projection::inverse(border.eastKm * 1000.0 + 0.1, border.northKm * 1000.0 + 500.0,
                             latitude, longitude);

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(latitude, longitude, height));
    CHECK_NEAR(height, 55.0, 1e-9);
}

} // namespace

int main()
{
    testTileKeyToString();
    testTileKeyFor();
    testCoverage();
    testSamplingUsesTheMatchingTile();
    testTilesAreCached();
    testOutsideCoverageFails();
    testMissingTileFails();
    testWithoutLoader();
    testBorderFallsBackToNeighbour();
    return geotest::summarize("BkgDgm5HeightDataSourceTests");
}
