#include "MongooseFetch.h"
extern "C" {
#include "mongoose.h"
}


#include "TestSupport.h"
#include <filesystem>
namespace fs = std::filesystem;

namespace {

    void testMongooseDownload(const char* url, const fs::path &targetPath)
    {
        printf("\n*** testMongooseDownload ***\n");

        printf(" fetchUrlMongoose(url,file)\n  url  : '%s'\n  file : '%s'\n\n",
            url,
            targetPath.string().c_str());

        // Ensure a clean slate in case a previous run left the file behind.
        std::error_code ec;
        fs::remove(targetPath, ec);

        bool ok = app::fetchUrlMongoose(url, targetPath.string(), "", "");
        CHECK_TRUE(ok);

        CHECK_TRUE(fs::exists(targetPath));
        CHECK_TRUE(fs::file_size(targetPath, ec) > 0 && !ec);

        // Cleanup
        //fs::remove(targetPath, ec);
    }

    void testMongooseDns()
    {
        printf("\n*** testMongooseDns ***\n");

        mg_mgr mgr;
        mg_mgr_init(&mgr);

        const char* dnsURL = "http://websocket.org/";

        mg_connection* c = mg_http_connect(&mgr, dnsURL, NULL, NULL);
        if (c == nullptr) {
            mg_mgr_free(&mgr);
            CHECK_TRUE(false);
            return;
        }

        mg_log("%M > %M", mg_print_ip_port, &c->loc, mg_print_ip_port, &c->rem);
        mg_log("%lu %s is %M", &c->id, dnsURL, mg_print_ip, &c->rem);

        for (int i = 0; i < 10; ++i) {
            mg_mgr_poll(&mgr, 100);
            printf("Polling Mongoose DNS connection... %i : %s\n", i, mgr.dns4.url);
        }

        mg_mgr_free(&mgr);
        CHECK_TRUE(true);
    }

} // namespace


int main(int argc, char* argv[])
{
    const fs::path tempDir = fs::temp_directory_path() / "SolarSimTests";
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    CHECK_TRUE(!ec);

    const fs::path targetPath_1 = tempDir / "test_download_bavaria_dgm1.zip";
    testMongooseDownload("https://download1.bayernwolke.de/a/dgm/dgm1xyz/655_5487.zip", targetPath_1);

    const fs::path targetPath_2 = tempDir / "test_download_world_copernicus_dem30.tif";
    testMongooseDownload("https://copernicus-dem-30m.s3.amazonaws.com/Copernicus_DSM_COG_10_N46_00_E009_00_DEM/Copernicus_DSM_COG_10_N46_00_E009_00_DEM.tif", targetPath_2);

    testMongooseDns();

    return geotest::summarize("MongooseTests");
}
