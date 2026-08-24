#include "geolib/HeightDataSourceRegistry.h"

#include "geolib/GridHeightDataSource.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace geo {

HeightDataSourceRegistry& HeightDataSourceRegistry::instance()
{
    static HeightDataSourceRegistry registry = [] {
        HeightDataSourceRegistry r;
        r.addSource(std::make_shared<FlatHeightDataSource>());
        return r;
    }();
    return registry;
}

void HeightDataSourceRegistry::addSource(HeightDataSourcePtr source)
{
    if (source) {
        m_sources.push_back(std::move(source));
    }
}

void HeightDataSourceRegistry::clear()
{
    m_sources.clear();
}

std::vector<HeightDataSourcePtr> HeightDataSourceRegistry::sourcesFor(double latitudeDeg,
                                                                     double longitudeDeg) const
{
    std::vector<HeightDataSourcePtr> matches;
    for (const auto& source : m_sources) {
        if (source->covers(latitudeDeg, longitudeDeg)) {
            matches.push_back(source);
        }
    }
    std::stable_sort(matches.begin(), matches.end(),
                     [](const HeightDataSourcePtr& a, const HeightDataSourcePtr& b) {
                         return a->resolutionM() < b->resolutionM();
                     });
    return matches;
}

HeightDataSourcePtr HeightDataSourceRegistry::selectSource(double latitudeDeg,
                                                           double longitudeDeg) const
{
    const auto matches = sourcesFor(latitudeDeg, longitudeDeg);
    return matches.empty() ? nullptr : matches.front();
}

HeightDataSourcePtr HeightDataSourceRegistry::selectSource(const GeoLocation& location) const
{
    return selectSource(location.latitude(), location.longitude());
}

} // namespace geo
