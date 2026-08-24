#include "geolib/data_sources/BavariaDgm1HeightDataSource.h"

#include "TestSupport.h"

#include "geolib/UtmProjection.h"

#include <cmath>
#include <map>
#include <memory>
#include <vector>

using namespace geo;

namespace {

using TileKey = BavariaDgm1HeightDataSource::TileKey;

/// Builds a 1 km tile whose height encodes the tile key, so a sample tells us
/// which tile answered. `extraNodes` extends the tile past its nominal square,
/// which mirrors real deliveries that repeat the shared edge.
std::shared_ptr<Utm32GridTile> makeTile(const TileKey& key, double baseHeight,
                                        int extraNodes = 0)
{
    constexpr double kSpacing = 100.0; // 11 nodes span the full kilometre
    const int nodes = 11 + extraNodes;
    std::vector<double> heights(static_cast<std::size_t>(nodes) * nodes, baseHeight);
    return std::make_shared<Utm32GridTile>(key.eastKm * 1000.0, key.northKm * 1000.0, kSpacing,
                                           nodes, nodes, std::move(heights));
}

void testTileKeyToString()
{
    CHECK_EQ_STR(TileKey{690, 5334}.toString(), "690_5334");
    CHECK_EQ_STR(TileKey{4, 12}.toString(), "4_12");
}

void testTileKeyOrdering()
{
    CHECK_TRUE(TileKey{690, 5334} < TileKey{691, 5334});
    CHECK_TRUE(TileKey{690, 5334} < TileKey{690, 5335});
    CHECK_FALSE(TileKey{690, 5334} < TileKey{690, 5334});
    CHECK_FALSE(TileKey{691, 5334} < TileKey{690, 9999});
}

void testCoverage()
{
    const BavariaDgm1HeightDataSource source({});
    CHECK_NEAR(source.resolutionM(), 1.0, 1e-12);
    CHECK_TRUE(source.covers(48.1372, 11.5756));  // Munich
    CHECK_TRUE(source.covers(49.4521, 11.0767));  // Nuremberg
    CHECK_FALSE(source.covers(52.5200, 13.4050)); // Berlin, outside Bavaria
    CHECK_FALSE(source.covers(48.8566, 2.3522));  // Paris
}

void testTileKeyForMatchesProjection()
{
    double easting = 0.0;
    double northing = 0.0;
    Utm32Projection::forward(48.1372, 11.5756, easting, northing);

    const TileKey key = BavariaDgm1HeightDataSource::tileKeyFor(48.1372, 11.5756);
    CHECK_EQ_INT(key.eastKm, static_cast<int>(std::floor(easting / 1000.0)));
    CHECK_EQ_INT(key.northKm, static_cast<int>(std::floor(northing / 1000.0)));

    // toUtm32 must stay consistent with the shared projection.
    double sourceEast = 0.0;
    double sourceNorth = 0.0;
    BavariaDgm1HeightDataSource::toUtm32(48.1372, 11.5756, sourceEast, sourceNorth);
    CHECK_NEAR(sourceEast, easting, 1e-9);
    CHECK_NEAR(sourceNorth, northing, 1e-9);
}

void testSampleUsesContainingTile()
{
    const TileKey expected = BavariaDgm1HeightDataSource::tileKeyFor(48.1372, 11.5756);
    int loadCalls = 0;

    BavariaDgm1HeightDataSource source([&](const TileKey& key) {
        ++loadCalls;
        return (key < expected || expected < key) ? nullptr : makeTile(key, 512.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(48.1372, 11.5756, height));
    CHECK_NEAR(height, 512.0, 1e-9);
    CHECK_EQ_INT(loadCalls, 1);
}

void testOutsideCoverageDoesNotLoad()
{
    int loadCalls = 0;
    BavariaDgm1HeightDataSource source([&](const TileKey& key) {
        ++loadCalls;
        return makeTile(key, 1.0);
    });

    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(52.5200, 13.4050, height)); // Berlin
    CHECK_EQ_INT(loadCalls, 0);
}

/// Missing tiles must be cached as well, so they are requested only once.
void testNegativeResultsAreCached()
{
    int loadCalls = 0;
    BavariaDgm1HeightDataSource source([&](const TileKey&) {
        ++loadCalls;
        return nullptr;
    });

    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(48.1372, 11.5756, height));
    const int afterFirst = loadCalls;
    CHECK_FALSE(source.sampleHeight(48.1372, 11.5756, height));
    CHECK_EQ_INT(loadCalls, afterFirst);
}

void testLoadedTilesAreCached()
{
    int loadCalls = 0;
    BavariaDgm1HeightDataSource source([&](const TileKey& key) {
        ++loadCalls;
        return makeTile(key, 300.0);
    });

    double height = 0.0;
    CHECK_TRUE(source.sampleHeight(48.1372, 11.5756, height));
    CHECK_EQ_INT(loadCalls, 1);

    // A nearby point inside the same tile must not trigger another load.
    CHECK_TRUE(source.sampleHeight(48.1373, 11.5757, height));
    CHECK_EQ_INT(loadCalls, 1);
}

void testMissingLoaderYieldsNoData()
{
    const BavariaDgm1HeightDataSource source({});
    double height = 0.0;
    CHECK_FALSE(source.sampleHeight(48.1372, 11.5756, height));
}

/// Right at a tile border the interpolation stencil reaches into the
/// neighbour, so the neighbouring tile has to be consulted.
void testBorderFallsBackToNeighbourTile()
{
    // Find a geodetic position that projects very close to a tile edge.
    double easting = 0.0;
    double northing = 0.0;
    Utm32Projection::forward(48.1372, 11.5756, easting, northing);
    const int eastKm = static_cast<int>(std::floor(easting / 1000.0));
    const int northKm = static_cast<int>(std::floor(northing / 1000.0));

    // 0.2 m east of the western edge of the tile: the stencil of the western
    // neighbour still covers it, the containing tile is intentionally absent.
    const double borderEasting = eastKm * 1000.0 + 0.2;
    double latitude = 0.0;
    double longitude = 0.0;
    Utm32Projection::inverse(borderEasting, northing, latitude, longitude);

    const TileKey missing{eastKm, northKm};
    const TileKey neighbour{eastKm - 1, northKm};

    std::map<std::string, int> requested;
    BavariaDgm1HeightDataSource source([&](const TileKey& key) -> std::shared_ptr<Utm32GridTile> {
        ++requested[key.toString()];
        if (!(key < neighbour) && !(neighbour < key)) {
            // The western neighbour is delivered with one extra node column, so
            // it still covers the shared edge and a little beyond.
            return makeTile(key, 777.0, 1);
        }
        return nullptr;
    });

    double height = 0.0;
    const bool ok = source.sampleHeight(latitude, longitude, height);
    CHECK_TRUE(ok);
    if (ok) {
        CHECK_NEAR(height, 777.0, 1e-9);
    }
    CHECK_EQ_INT(requested[missing.toString()], 1);
    CHECK_EQ_INT(requested[neighbour.toString()], 1);
}

} // namespace

int main()
{
    testTileKeyToString();
    testTileKeyOrdering();
    testCoverage();
    testTileKeyForMatchesProjection();
    testSampleUsesContainingTile();
    testOutsideCoverageDoesNotLoad();
    testNegativeResultsAreCached();
    testLoadedTilesAreCached();
    testMissingLoaderYieldsNoData();
    testBorderFallsBackToNeighbourTile();
    return geotest::summarize("BavariaDgm1HeightDataSourceTests");
}
