#include "GeoDataSources.h"

#include "geolib/GridHeightDataSource.h"
#include "geolib/HeightDataSourceRegistry.h"
#include "geolib/data_sources/BavariaDgm1HeightDataSource.h"
#include "geolib/data_sources/BavariaDgm1TileDownloader.h"
#include "geolib/data_sources/WorldCopernicusDem30HeightDataSource.h"
#include "geolib/data_sources/WorldCopernicusDem30TileDownloader.h"


void GeoDataSources::registerHeightDataSources()
{
    if (m_height_data_sources_registry) {
        return;
    }

    m_height_data_sources_registry = &(geo::HeightDataSourceRegistry::instance());

    for (const auto& source : m_height_data_sources_registry->sources()) {
      if (source->name().find("DGM1") != std::string::npos ||
        source->name().find("Copernicus") != std::string::npos) {
        // Already registered.
        return;
      }
    }

    // register the BavariaDgm1 HeightDataSource
    geo::BavariaDgm1TileDownloader::Config byConfig{};
    byConfig.cacheDirectory = cacheDirectoryFor("height_data_cache/bavaria_dgm1");
    byConfig.baseUrl = "https://download1.bayernwolke.de/a/dgm/dgm1xyz";
    byConfig.fileExtension = ".zip";
    byConfig.allowDownload = true;

    geo::BavariaDgm1TileDownloader::FetchFunction byFetch =
      [this](const std::string& url, const std::string& targetPath) -> bool {
          return fetchUrl(url, targetPath, {}, {});
      };

    auto byDownloader = new geo::BavariaDgm1TileDownloader(byConfig, byFetch);

    m_height_data_sources_registry->addSource(
      std::make_shared<geo::BavariaDgm1HeightDataSource>(byDownloader->tileLoader())
    );

    // register the WorldCopernicusDem30 HeightDataSource
    geo::WorldCopernicusDem30TileDownloader::Config worldConfig{};
    worldConfig.cacheDirectory = cacheDirectoryFor("height_data_cache/world_copernicus_dem30");
    worldConfig.baseUrl = "https://copernicus-dem-30m.s3.amazonaws.com";
    worldConfig.fileExtension = ".hgt";
    worldConfig.allowDownload = true;

    static const std::string kCopernicusUserName = "";
    static const std::string kCopernicusPassword = "";

    geo::WorldCopernicusDem30TileDownloader::FetchFunction worldFetch =
      [this](const std::string& url, const std::string& targetPath) -> bool {
        return fetchUrl(url, targetPath, kCopernicusUserName, kCopernicusPassword);
    };

    auto worldDownloader = new geo::WorldCopernicusDem30TileDownloader(worldConfig, worldFetch);

    m_height_data_sources_registry->addSource(
      std::make_shared<geo::WorldCopernicusDem30HeightDataSource>(worldDownloader->tileLoader())
    );
}
