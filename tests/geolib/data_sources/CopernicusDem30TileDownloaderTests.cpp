#include "geolib/data_sources/CopernicusDem30TileDownloader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace geo;

namespace {

using TileKey = CopernicusDem30TileDownloader::TileKey;

const char* kCacheDir = "copernicus_test_cache";

std::string cacheFile(const std::string& name)
{
    return std::string(kCacheDir) + "/" + name;
}

/// Writes a 3x3 HGT raster with a constant height.
void writeHgtTile(const std::string& path, int height)
{
    std::ofstream out(path, std::ios::binary);
    for (int i = 0; i < 9; ++i) {
        const char bytes[2] = {static_cast<char>((height >> 8) & 0xFF),
                               static_cast<char>(height & 0xFF)};
        out.write(bytes, 2);
    }
}

void removeCache()
{
    // The tests only ever create these files.
    const char* names[] = {"Copernicus_DSM_COG_10_N48_00_E011_00_DEM.hgt",
                           "Copernicus_DSM_COG_10_N49_00_E011_00_DEM.hgt",
                           "Copernicus_DSM_COG_10_N50_00_E011_00_DEM.hgt"};
    for (const char* name : names) {
        std::remove(cacheFile(name).c_str());
    }
}

CopernicusDem30TileDownloader::Config makeConfig()
{
    CopernicusDem30TileDownloader::Config config;
    config.cacheDirectory = kCacheDir;
    config.baseUrl = "https://example.invalid/glo30";
    return config;
}

void testFileNaming()
{
    const CopernicusDem30TileDownloader downloader(makeConfig());
    CHECK_EQ_STR(downloader.fileNameFor(TileKey{48, 11}),
                 "Copernicus_DSM_COG_10_N48_00_E011_00_DEM.hgt");
    CHECK_EQ_STR(downloader.urlFor(TileKey{48, 11}),
                 "https://example.invalid/glo30/Copernicus_DSM_COG_10_N48_00_E011_00_DEM/"
                 "Copernicus_DSM_COG_10_N48_00_E011_00_DEM.hgt");
    CHECK_EQ_STR(downloader.cachePathFor(TileKey{48, 11}),
                 "copernicus_test_cache/Copernicus_DSM_COG_10_N48_00_E011_00_DEM.hgt");
}

/// A trailing slash on the base URL must not produce a double slash.
void testUrlWithTrailingSlash()
{
    auto config = makeConfig();
    config.baseUrl = "https://example.invalid/glo30/";
    const CopernicusDem30TileDownloader downloader(config);
    CHECK_EQ_STR(downloader.urlFor(TileKey{48, 11}),
                 "https://example.invalid/glo30/Copernicus_DSM_COG_10_N48_00_E011_00_DEM/"
                 "Copernicus_DSM_COG_10_N48_00_E011_00_DEM.hgt");
}

void testDefaultConfig()
{
    const CopernicusDem30TileDownloader downloader;
    CHECK_TRUE(downloader.config().allowDownload);
    CHECK_FALSE(downloader.config().cacheDirectory.empty());
}

void testCachedFileIsUsedWithoutDownload()
{
    removeCache();
    const TileKey key{48, 11};

    int fetchCalls = 0;
    CopernicusDem30TileDownloader downloader(makeConfig(),
                                             [&](const std::string&, const std::string& target) {
                                                 ++fetchCalls;
                                                 writeHgtTile(target, 456);
                                                 return true;
                                             });

    // First access downloads...
    std::string error;
    const auto tile = downloader.loadTile(key, &error);
    CHECK_TRUE(tile != nullptr);
    CHECK_EQ_INT(fetchCalls, 1);
    if (tile) {
        double height = 0.0;
        CHECK_TRUE(tile->sampleHeight(48.5, 11.5, height));
        CHECK_NEAR(height, 456.0, 1e-9);
    }

    // ...the second access is served from the cache.
    CHECK_TRUE(downloader.loadTile(key, &error) != nullptr);
    CHECK_EQ_INT(fetchCalls, 1);

    removeCache();
}

void testDownloadDisabled()
{
    removeCache();
    auto config = makeConfig();
    config.allowDownload = false;

    int fetchCalls = 0;
    const CopernicusDem30TileDownloader downloader(config,
                                                   [&](const std::string&, const std::string&) {
                                                       ++fetchCalls;
                                                       return true;
                                                   });

    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{49, 11}, localPath, &error));
    CHECK_EQ_INT(fetchCalls, 0);
    CHECK_FALSE(error.empty());
}

void testMissingFetchFunction()
{
    removeCache();
    const CopernicusDem30TileDownloader downloader(makeConfig());
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{50, 11}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testFailedDownloadReportsError()
{
    removeCache();
    const CopernicusDem30TileDownloader downloader(makeConfig(),
                                                   [](const std::string&, const std::string&) {
                                                       return false; // simulate a 404 (ocean tile)
                                                   });
    std::string error;
    CHECK_TRUE(downloader.loadTile(TileKey{49, 11}, &error) == nullptr);
    CHECK_FALSE(error.empty());
}

/// A fetch that claims success but writes nothing must be detected.
void testLyingFetchIsDetected()
{
    removeCache();
    const CopernicusDem30TileDownloader downloader(makeConfig(),
                                                   [](const std::string&, const std::string&) {
                                                       return true; // but no file is created
                                                   });
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{50, 11}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testTileLoaderIntegration()
{
    removeCache();
    CopernicusDem30TileDownloader downloader(makeConfig(),
                                             [](const std::string&, const std::string& target) {
                                                 writeHgtTile(target, 321);
                                                 return true;
                                             });

    const auto loader = downloader.tileLoader();
    const auto tile = loader(TileKey{48, 11});
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        double height = 0.0;
        CHECK_TRUE(tile->sampleHeight(48.5, 11.5, height));
        CHECK_NEAR(height, 321.0, 1e-9);
    }

    removeCache();

    // An unavailable tile must surface as a null pointer.
    CopernicusDem30TileDownloader failing(
        makeConfig(), [](const std::string&, const std::string&) { return false; });
    CHECK_TRUE(failing.tileLoader()(TileKey{49, 11}) == nullptr);
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
    return geotest::summarize("CopernicusDem30TileDownloaderTests");
}
