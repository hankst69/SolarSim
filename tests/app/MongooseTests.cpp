#include "TestSupport.h"

extern "C" {
#include "mongoose.h"
}

namespace {


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

        for (int i = 0; i < 10; ++i) {
            mg_mgr_poll(&mgr, 100);
        }
        mg_mgr_free(&mgr);   
        CHECK_TRUE(true);
    }

} // namespace

int main(int argc, char* argv[])
{
    testMongooseDns();

    return geotest::summarize("MongooseTests");
}
