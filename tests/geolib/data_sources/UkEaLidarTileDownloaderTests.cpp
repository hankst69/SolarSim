#include "geolib/data_sources/UkEaLidarTileDownloader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace geo;

namespace {

using TileKey = UkEaLidarTileDownloader::TileKey;

const char* kCacheDir = "uk_lidar_test_cache";

std::string cacheFile(const std::string& name)
{
    return std::string(kCacheDir) + "/" + name;
}

/// Writes a small ASCII grid tile with a constant height.
void writeAscTile(const std::string& path, int originEast, int originNorth, double height)
{
    std::ofstream out(path);
    out << "ncols 3\nnrows 3\n"
        << "xllcorner " << originEast << "\n"
        << "yllcorner " << originNorth << "\n"
        << "cellsize 1\nNODATA_value -9999\n";
    for (int row = 0; row < 3; ++row) {
        out << height << " " << height << " " << height << "\n";
    }
}

void removeCache()
{
    // The tests only ever create these files.
    const char* names[] = {"LIDARCOMP-DTM-1M-sp00sw.asc", "LIDARCOMP-DTM-1M-sp00ne.asc",
                           "LIDARCOMP-DTM-1M-sp50sw.asc"};
    for (const char* name : names) {
        std::remove(cacheFile(name).c_str());
    }
}

UkEaLidarTileDownloader::Config makeConfig()
{
    UkEaLidarTileDownloader::Config config;
    config.cacheDirectory = kCacheDir;
    config.baseUrl = "https://example.invalid/lidar";
    return config;
}

void testFileNaming()
{
    const UkEaLidarTileDownloader downloader(makeConfig());
    CHECK_EQ_STR(downloader.fileNameFor(TileKey{400000, 200000}), "LIDARCOMP-DTM-1M-sp00sw.asc");
    CHECK_EQ_STR(downloader.urlFor(TileKey{400000, 200000}),
                 "https://example.invalid/lidar/LIDARCOMP-DTM-1M-sp00sw.asc");
    CHECK_EQ_STR(downloader.cachePathFor(TileKey{400000, 200000}),
                 "uk_lidar_test_cache/LIDARCOMP-DTM-1M-sp00sw.asc");
}

/// A trailing slash on the base URL must not produce a double slash.
void testUrlWithTrailingSlash()
{
    auto config = makeConfig();
    config.baseUrl = "https://example.invalid/lidar/";
    const UkEaLidarTileDownloader downloader(config);
    CHECK_EQ_STR(downloader.urlFor(TileKey{400000, 200000}),
                 "https://example.invalid/lidar/LIDARCOMP-DTM-1M-sp00sw.asc");
}

/// Tiles outside the lettered part of the National Grid have no file name.
void testTileOutsideTheGrid()
{
    const UkEaLidarTileDownloader downloader(makeConfig());
    const TileKey outside{-5000, 200000};
    CHECK_TRUE(downloader.fileNameFor(outside).empty());
    CHECK_TRUE(downloader.urlFor(outside).empty());

    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(outside, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testDefaultConfig()
{
    const UkEaLidarTileDownloader downloader;
    CHECK_TRUE(downloader.config().allowDownload);
    CHECK_FALSE(downloader.config().cacheDirectory.empty());
}

void testCachedFileIsUsedWithoutDownload()
{
    removeCache();
    const TileKey key{400000, 200000};

    int fetchCalls = 0;
    UkEaLidarTileDownloader downloader(makeConfig(),
                                       [&](const std::string&, const std::string& target) {
                                           ++fetchCalls;
                                           writeAscTile(target, 400000, 200000, 88.0);
                                           return true;
                                       });

    // First access downloads...
    BngGridTile tile;
    std::string error;
    CHECK_TRUE(downloader.loadTile(key, tile, &error));
    CHECK_EQ_INT(fetchCalls, 1);
    double height = 0.0;
    CHECK_TRUE(tile.sampleBng(400001.5, 200001.5, height));
    CHECK_NEAR(height, 88.0, 1e-9);

    // ...the second access is served from the cache.
    BngGridTile again;
    CHECK_TRUE(downloader.loadTile(key, again, &error));
    CHECK_EQ_INT(fetchCalls, 1);

    removeCache();
}

void testDownloadDisabled()
{
    removeCache();
    auto config = makeConfig();
    config.allowDownload = false;

    int fetchCalls = 0;
    const UkEaLidarTileDownloader downloader(config,
                                             [&](const std::string&, const std::string&) {
                                                 ++fetchCalls;
                                                 return true;
                                             });

    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{405000, 205000}, localPath, &error));
    CHECK_EQ_INT(fetchCalls, 0);
    CHECK_FALSE(error.empty());
}

void testMissingFetchFunction()
{
    removeCache();
    const UkEaLidarTileDownloader downloader(makeConfig());
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{450000, 200000}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testFailedDownloadReportsError()
{
    removeCache();
    const UkEaLidarTileDownloader downloader(makeConfig(),
                                             [](const std::string&, const std::string&) {
                                                 return false; // simulate a gap in the composite
                                             });
    BngGridTile tile;
    std::string error;
    CHECK_FALSE(downloader.loadTile(TileKey{405000, 205000}, tile, &error));
    CHECK_FALSE(error.empty());
}

/// A fetch that claims success but writes nothing must be detected.
void testLyingFetchIsDetected()
{
    removeCache();
    const UkEaLidarTileDownloader downloader(makeConfig(),
                                             [](const std::string&, const std::string&) {
                                                 return true; // but no file is created
                                             });
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{450000, 200000}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testTileLoaderIntegration()
{
    removeCache();
    UkEaLidarTileDownloader downloader(makeConfig(),
                                       [](const std::string&, const std::string& target) {
                                           writeAscTile(target, 400000, 200000, 12.0);
                                           return true;
                                       });

    const auto loader = downloader.tileLoader();
    const auto tile = loader(TileKey{400000, 200000});
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        double height = 0.0;
        CHECK_TRUE(tile->sampleBng(400001.5, 200001.5, height));
        CHECK_NEAR(height, 12.0, 1e-9);
    }

    removeCache();

    // An unavailable tile must surface as a null pointer, not as an empty tile.
    UkEaLidarTileDownloader failing(makeConfig(),
                                    [](const std::string&, const std::string&) { return false; });
    CHECK_TRUE(failing.tileLoader()(TileKey{405000, 205000}) == nullptr);
}

} // namespace

int main()
{
    testFileNaming();
    testUrlWithTrailingSlash();
    testTileOutsideTheGrid();
    testDefaultConfig();
    testCachedFileIsUsedWithoutDownload();
    testDownloadDisabled();
    testMissingFetchFunction();
    testFailedDownloadReportsError();
    testLyingFetchIsDetected();
    testTileLoaderIntegration();
    return geotest::summarize("UkEaLidarTileDownloaderTests");
}
