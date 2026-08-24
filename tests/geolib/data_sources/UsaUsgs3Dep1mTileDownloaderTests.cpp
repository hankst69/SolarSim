#include "geolib/data_sources/UsaUsgs3Dep1mTileDownloader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace geo;

namespace {

using TileKey = UsaUsgs3Dep1mTileDownloader::TileKey;

const char* kCacheDir = "usgs_3dep_test_cache";

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
    const char* names[] = {"USGS_1M_16_x540y4400.asc", "USGS_1M_16_x550y4410.asc",
                           "USGS_1M_13_x500y4400.asc"};
    for (const char* name : names) {
        std::remove(cacheFile(name).c_str());
    }
}

UsaUsgs3Dep1mTileDownloader::Config makeConfig()
{
    UsaUsgs3Dep1mTileDownloader::Config config;
    config.cacheDirectory = kCacheDir;
    config.baseUrl = "https://example.invalid/3dep";
    return config;
}

void testFileNaming()
{
    const UsaUsgs3Dep1mTileDownloader downloader(makeConfig());
    const TileKey key{16, 540000, 4400000};
    CHECK_EQ_STR(downloader.fileNameFor(key), "USGS_1M_16_x540y4400.asc");
    CHECK_EQ_STR(downloader.urlFor(key),
                 "https://example.invalid/3dep/16/TIFF/USGS_1M_16_x540y4400.asc");
    CHECK_EQ_STR(downloader.cachePathFor(key), "usgs_3dep_test_cache/USGS_1M_16_x540y4400.asc");
}

/// A trailing slash on the base URL must not produce a double slash.
void testUrlWithTrailingSlash()
{
    auto config = makeConfig();
    config.baseUrl = "https://example.invalid/3dep/";
    const UsaUsgs3Dep1mTileDownloader downloader(config);
    CHECK_EQ_STR(downloader.urlFor(TileKey{16, 540000, 4400000}),
                 "https://example.invalid/3dep/16/TIFF/USGS_1M_16_x540y4400.asc");
}

/// Tile keys without a valid UTM zone have no file name.
void testTileWithoutZone()
{
    const UsaUsgs3Dep1mTileDownloader downloader(makeConfig());
    const TileKey outside{0, 540000, 4400000};
    CHECK_TRUE(downloader.fileNameFor(outside).empty());
    CHECK_TRUE(downloader.urlFor(outside).empty());

    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(outside, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testDefaultConfig()
{
    const UsaUsgs3Dep1mTileDownloader downloader;
    CHECK_TRUE(downloader.config().allowDownload);
    CHECK_FALSE(downloader.config().cacheDirectory.empty());
}

void testCachedFileIsUsedWithoutDownload()
{
    removeCache();
    const TileKey key{16, 540000, 4400000};

    int fetchCalls = 0;
    UsaUsgs3Dep1mTileDownloader downloader(makeConfig(),
                                           [&](const std::string&, const std::string& target) {
                                               ++fetchCalls;
                                               writeAscTile(target, 540000, 4400000, 88.0);
                                               return true;
                                           });

    // First access downloads...
    UtmGridTile tile;
    std::string error;
    CHECK_TRUE(downloader.loadTile(key, tile, &error));
    CHECK_EQ_INT(fetchCalls, 1);
    CHECK_EQ_INT(tile.zone(), 16);
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(540001.5, 4400001.5, height));
    CHECK_NEAR(height, 88.0, 1e-9);

    // ...the second access is served from the cache.
    UtmGridTile again;
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
    const UsaUsgs3Dep1mTileDownloader downloader(config,
                                                 [&](const std::string&, const std::string&) {
                                                     ++fetchCalls;
                                                     return true;
                                                 });

    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{16, 550000, 4410000}, localPath, &error));
    CHECK_EQ_INT(fetchCalls, 0);
    CHECK_FALSE(error.empty());
}

void testMissingFetchFunction()
{
    removeCache();
    const UsaUsgs3Dep1mTileDownloader downloader(makeConfig());
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{13, 500000, 4400000}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testFailedDownloadReportsError()
{
    removeCache();
    const UsaUsgs3Dep1mTileDownloader downloader(makeConfig(),
                                                 [](const std::string&, const std::string&) {
                                                     return false; // simulate a coverage gap
                                                 });
    UtmGridTile tile;
    std::string error;
    CHECK_FALSE(downloader.loadTile(TileKey{16, 550000, 4410000}, tile, &error));
    CHECK_FALSE(error.empty());
}

/// A fetch that claims success but writes nothing must be detected.
void testLyingFetchIsDetected()
{
    removeCache();
    const UsaUsgs3Dep1mTileDownloader downloader(makeConfig(),
                                                 [](const std::string&, const std::string&) {
                                                     return true; // but no file is created
                                                 });
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{13, 500000, 4400000}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testTileLoaderIntegration()
{
    removeCache();
    UsaUsgs3Dep1mTileDownloader downloader(makeConfig(),
                                           [](const std::string&, const std::string& target) {
                                               writeAscTile(target, 540000, 4400000, 12.0);
                                               return true;
                                           });

    const auto loader = downloader.tileLoader();
    const auto tile = loader(TileKey{16, 540000, 4400000});
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        double height = 0.0;
        CHECK_TRUE(tile->sampleUtm(540001.5, 4400001.5, height));
        CHECK_NEAR(height, 12.0, 1e-9);
    }

    removeCache();

    // An unavailable tile must surface as a null pointer, not as an empty tile.
    UsaUsgs3Dep1mTileDownloader failing(
        makeConfig(), [](const std::string&, const std::string&) { return false; });
    CHECK_TRUE(failing.tileLoader()(TileKey{16, 550000, 4410000}) == nullptr);
}

} // namespace

int main()
{
    testFileNaming();
    testUrlWithTrailingSlash();
    testTileWithoutZone();
    testDefaultConfig();
    testCachedFileIsUsedWithoutDownload();
    testDownloadDisabled();
    testMissingFetchFunction();
    testFailedDownloadReportsError();
    testLyingFetchIsDetected();
    testTileLoaderIntegration();
    return geotest::summarize("UsaUsgs3Dep1mTileDownloaderTests");
}
