#pragma once

#include "geolib/TextureDataSource.h"

#include <vector>

namespace geo {

/// Registry of available Texture data sources with a selection mechanism based
/// on the geographic location of a GroundPlane.
///
/// Sources are ranked by coverage first and by resolution second, so the most
/// detailed data set covering the standpoint wins. A world wide fallback source
/// (e.g. DefaultTextureDataSource) can be registered as well; it is
/// only chosen if no better source covers the location.
///
class TextureDataSourceRegistry {
public:
    /// Registry pre-filled with the built-in sources (currently the flat
    /// fallback). Additional sources can be added by the application.
    static TextureDataSourceRegistry& instance();

    void addSource(TextureDataSourcePtr source);
    void clear();

    const std::vector<TextureDataSourcePtr>& sources() const { return m_sources; }

    /// All registered sources covering the given location, best (finest
    /// resolution) first.
    std::vector<TextureDataSourcePtr> sourcesFor(double latitudeDeg,
                                                double longitudeDeg) const;

    /// Best source for the given location, or nullptr if none covers it.
    TextureDataSourcePtr selectSource(double latitudeDeg, double longitudeDeg) const;
    TextureDataSourcePtr selectSource(const GeoLocation& location) const;

private:
    std::vector<TextureDataSourcePtr> m_sources;
};

} // namespace geo
