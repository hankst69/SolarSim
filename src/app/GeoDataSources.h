#pragma once

#include <string>
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/TextureDataSourceRegistry.h"

class GeoDataSources
{
public:
    void registerDataSources();

    geo::HeightDataSourceRegistry& heightDataSources();
    geo::TextureDataSourceRegistry& textureDataSources();

    bool fetchUrl(const std::string& url, const std::string& targetPath,
                  const std::string& userName, const std::string& password);

    std::string cacheDirectoryFor(const std::string& subDirectory);

private:
    void registerHeightDataSources();
    void registerTextureDataSources();
    geo::HeightDataSourceRegistry* m_height_data_sources_registry{ nullptr };
    geo::TextureDataSourceRegistry* m_texture_data_sources_registry{ nullptr };
};
