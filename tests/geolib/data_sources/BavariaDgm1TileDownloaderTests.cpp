#include "geolib/data_sources/BavariaDgm1TileDownloader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

using namespace geo;

namespace {

using TileKey = BavariaDgm1TileDownloader::TileKey;

const char* kCacheDir = "dgm1_test_cache";

std::string cacheFile(const std::string& name)
{
    return std::string(kCacheDir) + "/" + name;
}

void writeXyzTile(const std::string& path, double originEast, double originNorth, double height)
{
    std::ofstream out(path);
    out << std::fixed << std::setprecision(3);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            out << (originEast + column) << " " << (originNorth + row) << " " << height << "\n";
        }
    }
}

void removeCache()
{
    // The tests only ever create these files.
    const char* names[] = {"dgm1_32_690_5334_1_by.xyz", "dgm1_32_691_5334_1_by.xyz",
                           "dgm1_32_692_5334_1_by.xyz"};
    for (const char* name : names) {
        std::remove(cacheFile(name).c_str());
    }
}

BavariaDgm1TileDownloader::Config makeConfig()
{
    BavariaDgm1TileDownloader::Config config;
    config.cacheDirectory = kCacheDir;
    config.baseUrl = "https://example.invalid/dgm1";
    config.fileExtension = ".xyz";
    return config;
}

void testFileNaming()
{
    const BavariaDgm1TileDownloader downloader(makeConfig());
    CHECK_EQ_STR(downloader.fileNameFor(TileKey{690, 5334}), "dgm1_32_690_5334_1_by.xyz");
    CHECK_EQ_STR(downloader.urlFor(TileKey{690, 5334}),
                 "https://example.invalid/dgm1/dgm1_32_690_5334_1_by.xyz");
    CHECK_EQ_STR(downloader.cachePathFor(TileKey{690, 5334}),
                 "dgm1_test_cache/dgm1_32_690_5334_1_by.xyz");
}

/// A trailing slash on the base URL must not produce a double slash.
void testUrlWithTrailingSlash()
{
    auto config = makeConfig();
    config.baseUrl = "https://example.invalid/dgm1/";
    const BavariaDgm1TileDownloader downloader(config);
    CHECK_EQ_STR(downloader.urlFor(TileKey{690, 5334}),
                 "https://example.invalid/dgm1/dgm1_32_690_5334_1_by.xyz");
}

void testDefaultConfig()
{
    const BavariaDgm1TileDownloader downloader;
    CHECK_TRUE(downloader.config().allowDownload);
    CHECK_FALSE(downloader.config().cacheDirectory.empty());
}

void testCachedFileIsUsedWithoutDownload()
{
    removeCache();
    const BavariaDgm1TileDownloader prepare(makeConfig());
    // Create the cache directory by attempting a download that writes the file.
    const TileKey key{690, 5334};

    int fetchCalls = 0;
    BavariaDgm1TileDownloader downloader(makeConfig(),
                                         [&](const std::string&, const std::string& target) {
                                             ++fetchCalls;
                                             writeXyzTile(target, 690000.0, 5334000.0, 123.0);
                                             return true;
                                         });

    // First access downloads...
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(downloader.loadTile(key, tile, &error));
    CHECK_EQ_INT(fetchCalls, 1);
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690001.0, 5334001.0, height));
    CHECK_NEAR(height, 123.0, 1e-9);

    // ...the second access is served from the cache.
    Utm32GridTile again;
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
    const BavariaDgm1TileDownloader downloader(config,
                                               [&](const std::string&, const std::string&) {
                                                   ++fetchCalls;
                                                   return true;
                                               });

    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{691, 5334}, localPath, &error));
    CHECK_EQ_INT(fetchCalls, 0);
    CHECK_FALSE(error.empty());
}

void testMissingFetchFunction()
{
    removeCache();
    const BavariaDgm1TileDownloader downloader(makeConfig());
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{692, 5334}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testFailedDownloadReportsError()
{
    removeCache();
    const BavariaDgm1TileDownloader downloader(makeConfig(),
                                               [](const std::string&, const std::string&) {
                                                   return false; // simulate a 404
                                               });
    Utm32GridTile tile;
    std::string error;
    CHECK_FALSE(downloader.loadTile(TileKey{691, 5334}, tile, &error));
    CHECK_FALSE(error.empty());
}

/// A fetch that claims success but writes nothing must be detected.
void testLyingFetchIsDetected()
{
    removeCache();
    const BavariaDgm1TileDownloader downloader(makeConfig(),
                                               [](const std::string&, const std::string&) {
                                                   return true; // but no file is created
                                               });
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{692, 5334}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testTileLoaderIntegration()
{
    removeCache();
    BavariaDgm1TileDownloader downloader(makeConfig(),
                                         [](const std::string&, const std::string& target) {
                                             writeXyzTile(target, 690000.0, 5334000.0, 321.0);
                                             return true;
                                         });

    const auto loader = downloader.tileLoader();
    const auto tile = loader(TileKey{690, 5334});
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        double height = 0.0;
        CHECK_TRUE(tile->sampleUtm(690001.0, 5334001.0, height));
        CHECK_NEAR(height, 321.0, 1e-9);
    }

    removeCache();

    // An unavailable tile must surface as a null pointer, not as an empty tile.
    BavariaDgm1TileDownloader failing(makeConfig(),
                                      [](const std::string&, const std::string&) { return false; });
    CHECK_TRUE(failing.tileLoader()(TileKey{691, 5334}) == nullptr);
}

} // namespace

int main()
{
    testFileNaming();
    testUrlWithTrailingSlash();
    testDefaultConfig();
    testCachedFileIsUsedWithoutDownload();
    testDownloadDisabled();
    testMissingFetchFunction();
    testFailedDownloadReportsError();
    testLyingFetchIsDetected();
    testTileLoaderIntegration();
    removeCache();
    return geotest::summarize("BavariaDgm1TileDownloaderTests");
}
