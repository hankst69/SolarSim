#include "geolib/data_sources/GermanyDgm5TileDownloader.h"

#include "TestSupport.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <string>

using namespace geo;

namespace {

using TileKey = GermanyDgm5TileDownloader::TileKey;

const char* kCacheDir = "dgm5_test_cache";

std::string cacheFile(const std::string& name)
{
    return std::string(kCacheDir) + "/" + name;
}

/// Writes a small XYZ tile with the official 5 m spacing.
void writeXyzTile(const std::string& path, double originEast, double originNorth, double height)
{
    std::ofstream out(path);
    out << std::fixed << std::setprecision(3);
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            out << (originEast + column * 5) << " " << (originNorth + row * 5) << " " << height
                << "\n";
        }
    }
}

void removeCache()
{
    // The tests only ever create these files.
    const char* names[] = {"dgm5_32_690_5334_2.xyz", "dgm5_32_691_5334_2.xyz",
                           "dgm5_32_692_5334_2.xyz"};
    for (const char* name : names) {
        std::remove(cacheFile(name).c_str());
    }
}

GermanyDgm5TileDownloader::Config makeConfig()
{
    GermanyDgm5TileDownloader::Config config;
    config.cacheDirectory = kCacheDir;
    config.baseUrl = "https://example.invalid/dgm5";
    return config;
}

void testFileNaming()
{
    const GermanyDgm5TileDownloader downloader(makeConfig());
    CHECK_EQ_STR(downloader.fileNameFor(TileKey{690, 5334}), "dgm5_32_690_5334_2.xyz");
    CHECK_EQ_STR(downloader.urlFor(TileKey{690, 5334}),
                 "https://example.invalid/dgm5/dgm5_32_690_5334_2.xyz");
    CHECK_EQ_STR(downloader.cachePathFor(TileKey{690, 5334}),
                 "dgm5_test_cache/dgm5_32_690_5334_2.xyz");
}

/// A trailing slash on the base URL must not produce a double slash.
void testUrlWithTrailingSlash()
{
    auto config = makeConfig();
    config.baseUrl = "https://example.invalid/dgm5/";
    const GermanyDgm5TileDownloader downloader(config);
    CHECK_EQ_STR(downloader.urlFor(TileKey{690, 5334}),
                 "https://example.invalid/dgm5/dgm5_32_690_5334_2.xyz");
}

void testDefaultConfig()
{
    const GermanyDgm5TileDownloader downloader;
    CHECK_TRUE(downloader.config().allowDownload);
    CHECK_FALSE(downloader.config().cacheDirectory.empty());
}

void testCachedFileIsUsedWithoutDownload()
{
    removeCache();
    const TileKey key{690, 5334};

    int fetchCalls = 0;
    GermanyDgm5TileDownloader downloader(makeConfig(),
                                     [&](const std::string&, const std::string& target) {
                                         ++fetchCalls;
                                         writeXyzTile(target, 690000.0, 5334000.0, 456.0);
                                         return true;
                                     });

    // First access downloads...
    Utm32GridTile tile;
    std::string error;
    CHECK_TRUE(downloader.loadTile(key, tile, &error));
    CHECK_EQ_INT(fetchCalls, 1);
    CHECK_NEAR(tile.cellSize(), 5.0, 1e-12);
    double height = 0.0;
    CHECK_TRUE(tile.sampleUtm(690005.0, 5334005.0, height));
    CHECK_NEAR(height, 456.0, 1e-9);

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
    const GermanyDgm5TileDownloader downloader(config, [&](const std::string&, const std::string&) {
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
    const GermanyDgm5TileDownloader downloader(makeConfig());
    std::string localPath;
    std::string error;
    CHECK_FALSE(downloader.ensureLocalFile(TileKey{692, 5334}, localPath, &error));
    CHECK_FALSE(error.empty());
}

void testFailedDownloadReportsError()
{
    removeCache();
    const GermanyDgm5TileDownloader downloader(makeConfig(),
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
    const GermanyDgm5TileDownloader downloader(makeConfig(),
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
    GermanyDgm5TileDownloader downloader(makeConfig(),
                                     [](const std::string&, const std::string& target) {
                                         writeXyzTile(target, 690000.0, 5334000.0, 321.0);
                                         return true;
                                     });

    const auto loader = downloader.tileLoader();
    const auto tile = loader(TileKey{690, 5334});
    CHECK_TRUE(tile != nullptr);
    if (tile) {
        double height = 0.0;
        CHECK_TRUE(tile->sampleUtm(690005.0, 5334005.0, height));
        CHECK_NEAR(height, 321.0, 1e-9);
    }

    removeCache();

    // An unavailable tile must surface as a null pointer, not as an empty tile.
    GermanyDgm5TileDownloader failing(makeConfig(),
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
    return geotest::summarize("GermanyDgm5TileDownloaderTests");
}
