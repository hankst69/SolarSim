#include "geolib/HeightDataSourceRegistry.h"

#include "TestSupport.h"

#include "geolib/GridHeightDataSource.h"

#include <memory>
#include <string>
#include <vector>

using namespace geo;

namespace {

/// Test source covering an explicit box with a configurable resolution.
class BoxSource : public HeightDataSource {
public:
    BoxSource(std::string name, GeoBounds bounds, double resolution, double height)
        : m_name(std::move(name)), m_bounds(bounds), m_resolution(resolution), m_height(height)
    {
    }

    std::string name() const override { return m_name; }
    GeoBounds coverage() const override { return m_bounds; }
    double resolutionM() const override { return m_resolution; }

    bool sampleHeight(double latitudeDeg, double longitudeDeg, double& heightM) const override
    {
        if (!m_bounds.contains(latitudeDeg, longitudeDeg)) {
            return false;
        }
        heightM = m_height;
        return true;
    }

private:
    std::string m_name;
    GeoBounds m_bounds;
    double m_resolution;
    double m_height;
};

void testGeoBounds()
{
    const GeoBounds world = GeoBounds::world();
    CHECK_TRUE(world.contains(0.0, 0.0));
    CHECK_TRUE(world.contains(-89.0, 179.0));
    CHECK_TRUE(world.contains(GeoLocation(48.1372, 11.5756)));

    const GeoBounds bavaria{47.2, 50.6, 8.9, 13.9};
    CHECK_TRUE(bavaria.contains(48.1372, 11.5756));
    CHECK_FALSE(bavaria.contains(52.52, 13.405)); // Berlin
    CHECK_FALSE(bavaria.contains(48.1372, 2.35)); // wrong longitude

    // The edges belong to the box.
    CHECK_TRUE(bavaria.contains(47.2, 8.9));
    CHECK_TRUE(bavaria.contains(50.6, 13.9));
    CHECK_FALSE(bavaria.contains(47.1999, 11.0));
}

void testFlatSource()
{
    const FlatHeightDataSource flat(123.5);
    CHECK_TRUE(flat.covers(0.0, 0.0));
    CHECK_TRUE(flat.covers(-70.0, 150.0));

    double height = 0.0;
    CHECK_TRUE(flat.sampleHeight(48.1372, 11.5756, height));
    CHECK_NEAR(height, 123.5, 1e-12);

    // The default is sea level.
    CHECK_TRUE(FlatHeightDataSource().sampleHeight(10.0, 10.0, height));
    CHECK_NEAR(height, 0.0, 1e-12);
}

void testGridSourceInterpolation()
{
    // 3x3 raster over 1 degree; row 0 is the northern edge.
    // North row 30/31/32, middle 20/21/22, south 10/11/12.
    const std::vector<double> heights{30.0, 31.0, 32.0, 20.0, 21.0, 22.0, 10.0, 11.0, 12.0};
    const GridHeightDataSource grid("test", GeoBounds{48.0, 49.0, 11.0, 12.0}, 3, 3, heights, 1.0);

    double height = 0.0;
    // North west corner.
    CHECK_TRUE(grid.sampleHeight(49.0, 11.0, height));
    CHECK_NEAR(height, 30.0, 1e-9);
    // South west corner.
    CHECK_TRUE(grid.sampleHeight(48.0, 11.0, height));
    CHECK_NEAR(height, 10.0, 1e-9);
    // North east corner.
    CHECK_TRUE(grid.sampleHeight(49.0, 12.0, height));
    CHECK_NEAR(height, 32.0, 1e-9);
    // Centre.
    CHECK_TRUE(grid.sampleHeight(48.5, 11.5, height));
    CHECK_NEAR(height, 21.0, 1e-9);

    // Outside the coverage.
    CHECK_FALSE(grid.sampleHeight(50.0, 11.5, height));
    CHECK_FALSE(grid.sampleHeight(48.5, 13.0, height));

    CHECK_EQ_INT(grid.columns(), 3);
    CHECK_EQ_INT(grid.rows(), 3);
    CHECK_NEAR(grid.resolutionM(), 1.0, 1e-12);
}

void testGridSourceNoData()
{
    const std::vector<double> heights{1.0, 2.0, 3.0, -32768.0};
    const GridHeightDataSource grid("test", GeoBounds{0.0, 1.0, 0.0, 1.0}, 2, 2, heights, 1.0);

    double height = 0.0;
    // Every stencil of this 2x2 raster touches the no-data cell.
    CHECK_FALSE(grid.sampleHeight(0.5, 0.5, height));
}

void testGridSourceDegenerate()
{
    double height = 0.0;
    // Fewer than two columns/rows cannot be interpolated.
    const GridHeightDataSource tiny("tiny", GeoBounds{0.0, 1.0, 0.0, 1.0}, 1, 1, {5.0}, 1.0);
    CHECK_FALSE(tiny.sampleHeight(0.5, 0.5, height));
}

void testRegistryAddAndClear()
{
    HeightDataSourceRegistry registry;
    CHECK_TRUE(registry.sources().empty());

    registry.addSource(std::make_shared<FlatHeightDataSource>());
    CHECK_EQ_INT(static_cast<long long>(registry.sources().size()), 1);

    // Null sources are ignored.
    registry.addSource(nullptr);
    CHECK_EQ_INT(static_cast<long long>(registry.sources().size()), 1);

    registry.clear();
    CHECK_TRUE(registry.sources().empty());
}

void testRegistrySelectsFinestResolution()
{
    HeightDataSourceRegistry registry;
    const GeoBounds bavaria{47.2, 50.6, 8.9, 13.9};

    registry.addSource(std::make_shared<FlatHeightDataSource>(0.0, 1000.0));
    registry.addSource(std::make_shared<BoxSource>("DGM10", bavaria, 10.0, 500.0));
    registry.addSource(std::make_shared<BoxSource>("DGM1", bavaria, 1.0, 501.0));

    // Inside Bavaria the 1 m source wins.
    const auto best = registry.selectSource(48.1372, 11.5756);
    CHECK_TRUE(best != nullptr);
    if (best) {
        CHECK_EQ_STR(best->name(), "DGM1");
    }

    // All three cover the location, ordered by resolution.
    const auto matches = registry.sourcesFor(48.1372, 11.5756);
    CHECK_EQ_INT(static_cast<long long>(matches.size()), 3);
    if (matches.size() == 3) {
        CHECK_NEAR(matches[0]->resolutionM(), 1.0, 1e-12);
        CHECK_NEAR(matches[1]->resolutionM(), 10.0, 1e-12);
        CHECK_NEAR(matches[2]->resolutionM(), 1000.0, 1e-12);
    }

    // Outside Bavaria only the world wide fallback remains.
    const auto fallback = registry.selectSource(52.52, 13.405);
    CHECK_TRUE(fallback != nullptr);
    if (fallback) {
        CHECK_NEAR(fallback->resolutionM(), 1000.0, 1e-12);
    }
    CHECK_EQ_INT(static_cast<long long>(registry.sourcesFor(52.52, 13.405).size()), 1);
}

void testRegistryWithoutCoverage()
{
    HeightDataSourceRegistry registry;
    registry.addSource(
        std::make_shared<BoxSource>("local", GeoBounds{47.0, 48.0, 11.0, 12.0}, 1.0, 100.0));

    CHECK_TRUE(registry.selectSource(47.5, 11.5) != nullptr);
    CHECK_TRUE(registry.selectSource(0.0, 0.0) == nullptr);
    CHECK_TRUE(registry.sourcesFor(0.0, 0.0).empty());
}

void testRegistrySelectByGeoLocation()
{
    HeightDataSourceRegistry registry;
    registry.addSource(std::make_shared<FlatHeightDataSource>());
    const auto source = registry.selectSource(GeoLocation(48.1372, 11.5756));
    CHECK_TRUE(source != nullptr);
}

/// The shared instance is pre-filled with the flat fallback.
void testSharedInstance()
{
    auto& registry = HeightDataSourceRegistry::instance();
    CHECK_TRUE(&registry == &HeightDataSourceRegistry::instance());
    CHECK_TRUE(registry.selectSource(48.1372, 11.5756) != nullptr);
    // A source is available anywhere on earth.
    CHECK_TRUE(registry.selectSource(-45.0, 170.0) != nullptr);
}

} // namespace

int main()
{
    testGeoBounds();
    testFlatSource();
    testGridSourceInterpolation();
    testGridSourceNoData();
    testGridSourceDegenerate();
    testRegistryAddAndClear();
    testRegistrySelectsFinestResolution();
    testRegistryWithoutCoverage();
    testRegistrySelectByGeoLocation();
    testSharedInstance();
    return geotest::summarize("HeightDataSourceTests");
}
