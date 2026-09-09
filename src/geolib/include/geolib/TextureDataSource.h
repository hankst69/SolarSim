#pragma once

#include "geolib/GeoDataSource.h"

#include <memory>
#include <string>

namespace geo {

/// Abstract provider of terrain Textures ("digital elevation/terrain model").
///
/// Implementations wrap a concrete data set, e.g. the Bavarian open data
/// "Digitales Gelaendemodell 1m (DGM1)", SRTM, Copernicus DEM, USGS 3DEP, ...
class TextureDataSource : GeoDataSource {
public:
    virtual ~TextureDataSource() = default;

    /// Terrain Texture in metres above the reference surface at the given
    /// location. Returns false if no value is available (data gap, tile not
    /// downloaded, outside coverage, ...).
    virtual bool sampleTexture(double latitudeDeg, double longitudeDeg,
                              unsigned char* &texture) const = 0;

    // implement GeoDataSource interface to redirect to specific TextureDataSource interface
    virtual bool sampleTexture(double latitudeDeg, double longitudeDeg, unsigned char* &texture) {
        return sampleTexture(latitudeDeg, longitudeDeg, texture);
    }
};

using TextureDataSourcePtr = std::shared_ptr<TextureDataSource>;

} // namespace geo
