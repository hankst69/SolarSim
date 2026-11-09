#include "TestSupport.h"

extern "C" {
#include "mongoose.h"
}
#include <filesystem>
#include "MongooseFetch.h"


namespace {

    void testMongooseDownload()
    {
        namespace fs = std::filesystem;

        const fs::path tempDir = fs::temp_directory_path() / "SolarSimTests";
        std::error_code ec;
        fs::create_directories(tempDir, ec);
        CHECK_TRUE(!ec);

        const fs::path targetPath = tempDir / "test_download.zip";

        // Ensure a clean slate in case a previous run left the file behind.
        fs::remove(targetPath, ec);

        //https://download1.bayernwolke.de/a/dgm/dgm1xyz/655_5487.zip
        bool ok = app::fetchUrlMongoose("https://download1.bayernwolke.de/a/dgm/dgm1xyz/655_5487.zip", targetPath.string(), "", "");
        CHECK_TRUE(ok);

        CHECK_TRUE(fs::exists(targetPath));
        CHECK_TRUE(fs::file_size(targetPath, ec) > 0 && !ec);

        // Cleanup
        fs::remove(targetPath, ec);
    }

    void testMongooseDns()
    {
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
    testMongooseDownload();

    //testMongooseDns();

    return geotest::summarize("MongooseTests");
}
