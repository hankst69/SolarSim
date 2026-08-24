#include "geolib/data_sources/UsaUsgs3Dep1mHeightDataSource.h"

#include "geolib/UtmProjection.h"
#include "TestSupport.h"

#include <memory>
#include <vector>

using namespace geo;

namespace {

using TileKey = UsaUsgs3Dep1mHeightDataSource::TileKey;

// Denver, Colorado - UTM zone 13N.
constexpr double kLat = 39.7392;
constexpr double kLon = -104.9903;

/// Tile of constant height covering the whole 10 km block of the key.
std::shared_ptr<UtmGridTile> makeTile(const TileKey& key, double height)
{
    // 11 samples of 1000 m spacing span the 10 km block.
    return std::make_shared<UtmGridTile>(key.zone, static_cast<double>(key.eastingM),
                                         static_cast<double>(key.northingM), 1000.0, 11, 11,
                                         std::vector<double>(121, height));
}

void testTileNaming()
{
    CHECK_EQ_STR((TileKey{16, 540000, 4400000}.toString()), "16_x540y4400");
    CHECK_EQ_STR((TileKey{13, 500000, 4400000}.toString()), "13_x500y4400");
    // An invalid zone has no official name.
    CHECK_TRUE(TileKey{0, 540000, 4400000}.toString().empty());
    CHECK_TRUE(TileKey{61, 540000, 4400000}.toString().empty());
}

void testTileKeyForSnapsToTheBlock()
{
    const auto key = UsaUsgs3Dep1mHeightDataSource::tileKeyFor(kLat, kLon);
    CHECK_EQ_INT(key.zone, 13);
    CHECK_EQ_INT(key.eastingM % 10000, 0);
    CHECK_EQ_INT(key.northingM % 10000, 0);

    int zone = 0;
    double easting = 0.0;
    double northing = 0.0;
    UsaUsgs3Dep1mHeightDataSource::toUtm(kLat, kLon, zone, easting, northing);
    CHECK_EQ_INT(zone, UtmProjection::zoneForLongitude(kLon));
    CHECK_TRUE(easting >= key.eastingM && easting < key.eastingM + 10000.0);
    CHECK_TRUE(northing >= key.northingM && northing < key.northingM + 10000.0);
}

void testCoverage()
{
    const UsaUsgs3Dep1mHeightDataSource source({});
    CHECK_TRUE(source.covers(kLat, kLon));
    CHECK_TRUE(source.covers(21.3, -157.8));      // Hawaii
    CHECK_FALSE(source.covers(48.1372, 11.5756)); // Munich
    CHECK_FALSE(source.covers(51.5, -1.0));       // England
    CHECK_NEAR(source.resolutionM(), 1.0, 1e-12);
    CHECK_FALSE(source.name().empty());
}

void testSamplingUsesTheMatchingTile()
{
    const auto expected = UsaUsgs3Dep1mHeightDataSource::tileKeyFor(kLat, kLon);
    UsaUsgs3Dep1mHeightDataSource source([&](const TileKey& key) {
        const bool match = key.zone == expected.zone && key.eastingM == expected.eastingM &&
                           key.northingM == expected.northingM;
        return makeTile(key, match ? 1600.0 : 5.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(kLat, kLon, height));
    CHECK_NEAR(height, 1600.0, 1e-9);
}

/// Tiles must only be requested once, including tiles that are unavailable.
void testTilesAreCached()
{
    int loads = 0;
    UsaUsgs3Dep1mHeightDataSource source([&](const TileKey& key) {
        ++loads;
        return makeTile(key, 1600.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(kLat, kLon, height));
    CHECK_TRUE(source.sampleHeight(kLat, kLon, height));
    CHECK_EQ_INT(loads, 1);

    int missing = 0;
    UsaUsgs3Dep1mHeightDataSource gaps([&](const TileKey&) {
        ++missing;
        return nullptr;
    });
    CHECK_FALSE(gaps.sampleHeight(kLat, kLon, height));
    CHECK_FALSE(gaps.sampleHeight(kLat, kLon, height));
    CHECK_EQ_INT(missing, 1);
}

void testOutsideCoverageFails()
{
    UsaUsgs3Dep1mHeightDataSource source([](const TileKey& key) { return makeTile(key, 10.0); });
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(48.1372, 11.5756, height));
}

void testWithoutLoader()
{
    const UsaUsgs3Dep1mHeightDataSource source({});
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(kLat, kLon, height));
}

/// 3DEP 1 m coverage has gaps; a missing tile must be reported, not guessed.
void testMissingTileFails()
{
    UsaUsgs3Dep1mHeightDataSource source([](const TileKey&) { return nullptr; });
    double height = 123.0;
    CHECK_FALSE(source.sampleHeight(kLat, kLon, height));
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
    return geotest::summarize("UsaUsgs3Dep1mHeightDataSourceTests");
}
