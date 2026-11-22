#pragma once

#include <string>
#include "geolib/ProgressClass.h"

#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/TextureDataSourceRegistry.h"

#include "geolib/data_sources/BavariaDgm1TileDownloader.h"
#include "geolib/data_sources/WorldCopernicusDem30TileDownloader.h"

namespace app {

class GeoDataSources
{
public:
    GeoDataSources();
    ~GeoDataSources();

    void registerDataSources();
    geo::HeightDataSourceRegistry& heightDataSources()   {return *(m_height_data_sources_registry);}
    geo::TextureDataSourceRegistry& textureDataSources() {return *(m_texture_data_sources_registry);}

    bool fetchUrl(const std::string& url, const std::string& targetPath,
                  const std::string& userName, const std::string& password);

    std::string cacheDirectoryFor(const std::string& subDirectory);

private:
    geo::HeightDataSourceRegistry* m_height_data_sources_registry{ nullptr };
    geo::TextureDataSourceRegistry* m_texture_data_sources_registry{ nullptr };
    // The downloaders are kept alive as long as the process, because the
    // HeightDataSourceRegistry is a singleton and registered sources keep a
    // reference (via their TileLoader callback) to the downloader that
    // created them. If these were regular (non-static) members, a
    // short-lived GeoDataSources instance would register sources backed by
    // a downloader that gets destroyed as soon as that instance goes out of
    // scope, leaving dangling references (and an effectively empty Config)
    // behind for any later GeoDataSources instance that finds the sources
    // already registered and skips re-creating its own downloaders.
    static std::shared_ptr<geo::BavariaDgm1TileDownloader> m_bavariaDgm1TileDownloader;
    static std::shared_ptr<geo::WorldCopernicusDem30TileDownloader> m_worldDem30TileDownloader;

public:
    void setProgressClass(geo::ProgressClassPtr progressClass)
    {
        m_progressClass = progressClass; //std::move(progressClass);
        if (m_height_data_sources_registry) {
            for (auto& source : m_height_data_sources_registry->sources()) {
                source->setProgressClass(progressClass);
            }
        }
        if (m_texture_data_sources_registry) {
            for (auto& source : m_texture_data_sources_registry->sources()) {
                source->setProgressClass(progressClass);
            }
        }
    }
protected:
    void progress(int percent, std::string msg) const
    {
        if (m_progressClass) {
            int progressVal = percent % 100;
            m_progressClass->progress(progressVal, std::move(msg));
        }
    }
    bool isCanceled() const
    {
        if (m_progressClass) {
            return m_progressClass->isCanceled();
        }
        return false;
    }
private:
    geo::ProgressClassPtr m_progressClass;
    int m_progressCount{0};
};

} // namespace