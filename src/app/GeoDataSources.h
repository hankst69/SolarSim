#pragma once

#include <string>
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/TextureDataSourceRegistry.h"

#include "geolib/data_sources/BavariaDgm1TileDownloader.h"
#include "geolib/data_sources/WorldCopernicusDem30TileDownloader.h"

class GeoDataSources
{
public:
    void registerDataSources();
    geo::HeightDataSourceRegistry& heightDataSources()   {return *(m_height_data_sources_registry);}
    geo::TextureDataSourceRegistry& textureDataSources() {return *(m_texture_data_sources_registry);}

    bool fetchUrl(const std::string& url, const std::string& targetPath,
                  const std::string& userName, const std::string& password);

    std::string cacheDirectoryFor(const std::string& subDirectory);

private:
    geo::HeightDataSourceRegistry* m_height_data_sources_registry{ nullptr };
    geo::TextureDataSourceRegistry* m_texture_data_sources_registry{ nullptr };
    std::shared_ptr<geo::BavariaDgm1TileDownloader> m_bavariaDgm1TileDownloader{ nullptr };
    std::shared_ptr<geo::WorldCopernicusDem30TileDownloader> m_worldDem30TileDownloader{ nullptr };

public:
    void setProgressClass(std::shared_ptr<geo::GeoDataSource::ProgressClass> progressClass)
    {
        m_progressClass = std::move(progressClass);
    }
protected:
    void progress(int percent, std::string msg) const
    {
        if (m_progressClass) {
            int progressVal = percent % 100;
            m_progressClass->progress(progressVal, std::move(msg));
        }
    }
private:
    std::shared_ptr<geo::GeoDataSource::ProgressClass> m_progressClass;
    int m_progressCount{0};
};
