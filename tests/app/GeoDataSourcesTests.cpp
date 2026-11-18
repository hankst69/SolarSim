#include "../app/GeoDataSources.h"

#include "TestSupport.h"

namespace {

/// registerHeightDataSources() must add the Bavaria DGM1 and World
/// Copernicus DEM30 sources exactly once, even if called repeatedly.
void testRegistersSourcesOnce()
{
    auto sources = new app::GeoDataSources();
    geo::HeightDataSourceRegistry& registry = sources->heightDataSources();
    //geo::HeightDataSourceRegistry& registry = geo::HeightDataSourceRegistry::instance();

    registry.clear();

    const std::size_t countBeforeFirstCall = registry.sources().size();
    CHECK_TRUE(countBeforeFirstCall == 0);

    sources->registerDataSources();
    const std::size_t countAfterFirstCall = registry.sources().size();
    CHECK_TRUE(countAfterFirstCall >= 2);

    sources->registerDataSources();
    CHECK_EQ_INT(static_cast<long long>(registry.sources().size()),
                static_cast<long long>(countAfterFirstCall));

    bool hasDgm1 = false;
    bool hasCopernicus = false;
    for (const auto& source : registry.sources()) {
        if (source->name().find("DGM1") != std::string::npos) {
            hasDgm1 = true;
        }
        if (source->name().find("Copernicus") != std::string::npos) {
            hasCopernicus = true;
        }
    }
    CHECK_TRUE(hasDgm1);
    CHECK_TRUE(hasCopernicus);

    registry.clear();
}

/// A location inside the Bavaria DGM1 coverage area must select the DGM1
/// source over the world wide Copernicus fallback.
void testSelectsBavariaDgm1ForBavariaLocation()
{
    auto sources = new app::GeoDataSources();
    geo::HeightDataSourceRegistry& registry = sources->heightDataSources();

    registry.clear();
    sources->registerDataSources();

    // Munich, inside Bavaria DGM1 coverage.
    const auto selected = registry.selectSource(48.1372, 11.5756);
    CHECK_TRUE(selected != nullptr);
    CHECK_TRUE(selected->name().find("DGM1") != std::string::npos);

    registry.clear();
}

/// A location outside the Bavaria DGM1 coverage area must fall back to the
/// world wide Copernicus DEM30 source.
void testSelectsCopernicusForNonBavariaLocation()
{
    auto sources = new app::GeoDataSources();
    geo::HeightDataSourceRegistry& registry = sources->heightDataSources();

    registry.clear();
    sources->registerDataSources();

    // Berlin, outside Bavaria.
    const auto selected = registry.selectSource(52.5200, 13.4050);
    CHECK_TRUE(selected != nullptr);
    CHECK_TRUE(selected->name().find("Copernicus") != std::string::npos);

    registry.clear();
}

/// The Bavaria DGM1 source selected for a Bavarian location must be able to
/// actually sample a height from a loaded tile.
void testBavariaDgm1SamplesHeight()
{
    auto sources = new app::GeoDataSources();
    geo::HeightDataSourceRegistry& registry = sources->heightDataSources();
    registry.clear();
    sources->registerDataSources();

    // Munich, inside Bavaria DGM1 coverage.
    const double latitude = 48.1372;
    const double longitude = 11.5756;
    const auto selected = registry.selectSource(latitude, longitude);
    CHECK_TRUE(selected != nullptr);

    double height = 0.0;
    CHECK_TRUE(selected->sampleHeight(latitude, longitude, height));
    CHECK_NEAR(height, 516.0, 1e-1);
}

/// The Copernicus DEM30 source selected for a non Bavarian location must be
/// able to actually sample a height from a loaded tile.
void testCopernicusSamplesHeight()
{
    auto sources = new app::GeoDataSources();
    geo::HeightDataSourceRegistry& registry = sources->heightDataSources();
    registry.clear();
    sources->registerDataSources();

    // Berlin, outside Bavaria.
    const double latitude = 52.5200;
    const double longitude = 13.4050;
    const auto selected = registry.selectSource(latitude, longitude);
    CHECK_TRUE(selected != nullptr);

    double height = 0.0;
    CHECK_TRUE(selected->sampleHeight(latitude, longitude, height));
    CHECK_NEAR(height, 37.1, 1e-1);
}

} // namespace

int main(int argc, char* argv[])
{
    testRegistersSourcesOnce();
    testSelectsBavariaDgm1ForBavariaLocation();
    testSelectsCopernicusForNonBavariaLocation();
    testBavariaDgm1SamplesHeight();
    testCopernicusSamplesHeight();

    return geotest::summarize("GeoDataSourcesTests");
}
