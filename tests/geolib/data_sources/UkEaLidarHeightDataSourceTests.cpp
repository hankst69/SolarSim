#include "geolib/data_sources/UkEaLidarHeightDataSource.h"

#include "geolib/BritishNationalGridProjection.h"
#include "TestSupport.h"

#include <memory>
#include <vector>

using namespace geo;

namespace {

using TileKey = UkEaLidarHeightDataSource::TileKey;

/// Tile of constant height covering the whole 5 km block of the key.
std::shared_ptr<BngGridTile> makeTile(const TileKey& key, double height)
{
    // 6 samples of 1000 m spacing span the 5 km block.
    return std::make_shared<BngGridTile>(static_cast<double>(key.eastingM),
                                         static_cast<double>(key.northingM), 1000.0, 6, 6,
                                         std::vector<double>(36, height));
}

void testTileNaming()
{
    // South west corner of SP -> "sp00sw".
    CHECK_EQ_STR(TileKey{400000, 200000}.toString(), "sp00sw");
    // 5 km east and north of it -> the north east quadrant of the same cell.
    CHECK_EQ_STR(TileKey{405000, 205000}.toString(), "sp00ne");
    CHECK_EQ_STR(TileKey{405000, 200000}.toString(), "sp00se");
    CHECK_EQ_STR(TileKey{400000, 205000}.toString(), "sp00nw");
    // 50 km east / 0 km north inside SP -> "sp50sw".
    CHECK_EQ_STR(TileKey{450000, 200000}.toString(), "sp50sw");
    // Inside TQ (London).
    CHECK_EQ_STR(TileKey{530000, 180000}.toString(), "tq38sw");
}

void testTileKeyForSnapsToTheBlock()
{
    const auto key = UkEaLidarHeightDataSource::tileKeyFor(51.5, -1.0);
    CHECK_EQ_INT(key.eastingM % 5000, 0);
    CHECK_EQ_INT(key.northingM % 5000, 0);

    double easting = 0.0;
    double northing = 0.0;
    UkEaLidarHeightDataSource::toBng(51.5, -1.0, easting, northing);
    CHECK_TRUE(easting >= key.eastingM && easting < key.eastingM + 5000.0);
    CHECK_TRUE(northing >= key.northingM && northing < key.northingM + 5000.0);
}

void testCoverage()
{
    const UkEaLidarHeightDataSource source({});
    CHECK_TRUE(source.covers(51.5, -1.0));
    CHECK_FALSE(source.covers(48.1372, 11.5756)); // Munich
    CHECK_FALSE(source.covers(51.5, 5.0));        // Netherlands
    CHECK_NEAR(source.resolutionM(), 1.0, 1e-12);
    CHECK_FALSE(source.name().empty());
}

void testSamplingUsesTheMatchingTile()
{
    const auto expected = UkEaLidarHeightDataSource::tileKeyFor(51.5, -1.0);
    UkEaLidarHeightDataSource source([&](const TileKey& key) {
        return makeTile(key, key.eastingM == expected.eastingM &&
                                     key.northingM == expected.northingM
                                 ? 120.0
                                 : 5.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(51.5, -1.0, height));
    CHECK_NEAR(height, 120.0, 1e-9);
}

/// Tiles must only be requested once, including tiles that are unavailable.
void testTilesAreCached()
{
    int loads = 0;
    UkEaLidarHeightDataSource source([&](const TileKey& key) {
        ++loads;
        return makeTile(key, 33.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(51.5, -1.0, height));
    CHECK_TRUE(source.sampleHeight(51.5, -1.0, height));
    CHECK_EQ_INT(loads, 1);

    int missing = 0;
    UkEaLidarHeightDataSource gaps([&](const TileKey&) {
        ++missing;
        return nullptr;
    });
    CHECK_FALSE(gaps.sampleHeight(51.5, -1.0, height));
    CHECK_FALSE(gaps.sampleHeight(51.5, -1.0, height));
    CHECK_EQ_INT(missing, 1);
}

void testOutsideCoverageFails()
{
    UkEaLidarHeightDataSource source([](const TileKey& key) { return makeTile(key, 10.0); });
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(48.1372, 11.5756, height));
}

void testWithoutLoader()
{
    const UkEaLidarHeightDataSource source({});
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(51.5, -1.0, height));
}

/// The composite has gaps; a missing tile must be reported, not guessed.
void testMissingTileFails()
{
    UkEaLidarHeightDataSource source([](const TileKey&) { return nullptr; });
    double height = 123.0;
    CHECK_FALSE(source.sampleHeight(51.5, -1.0, height));
}

} // namespace

int main()
{
    testTileNaming();
    testTileKeyForSnapsToTheBlock();
    testCoverage();
    testSamplingUsesTheMatchingTile();
    testTilesAreCached();
    testOutsideCoverageFails();
    testWithoutLoader();
    testMissingTileFails();
    return geotest::summarize("UkEaLidarHeightDataSourceTests");
}
