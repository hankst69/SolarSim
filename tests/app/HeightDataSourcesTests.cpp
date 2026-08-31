#include "HeightDataSources.h"

#include "geolib/HeightDataSourceRegistry.h"

#include "TestSupport.h"

#include <QCoreApplication>

#include <string>

namespace {

/// registerHeightDataSources() must add the Bavaria DGM1 and World
/// Copernicus DEM30 sources exactly once, even if called repeatedly.
void testRegistersSourcesOnce()
{
    geo::HeightDataSourceRegistry& registry = geo::HeightDataSourceRegistry::instance();
    registry.clear();

    registerHeightDataSources();
    const std::size_t countAfterFirstCall = registry.sources().size();
    CHECK_TRUE(countAfterFirstCall >= 2);

    registerHeightDataSources();
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

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    testRegistersSourcesOnce();

    return geotest::summarize("HeightDataSourcesTests");
}
