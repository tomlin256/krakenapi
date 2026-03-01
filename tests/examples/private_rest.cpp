#include <stdexcept>

#include <spdlog/spdlog.h>

#include "kraken_rest_client.hpp"

using namespace kraken::rest;

int main(int argc, char* argv[])
{
    curl_global_init(CURL_GLOBAL_ALL);

    try {
        KrakenRestClient client;
        auto creds = Credentials::from_file("default");

        auto resp = client.execute(GetWebSocketsTokenRequest{}, creds);
        if (resp.ok && resp.result) {
            spdlog::info("token:   {}", resp.result->token);
            spdlog::info("expires: {}", resp.result->expires);
        } else {
            for (const auto& e : resp.errors)
                spdlog::error("Error: {}", e);
        }
    }
    catch (std::exception& e) {
        spdlog::error("Error: {}", e.what());
    }
    catch (...) {
        spdlog::error("Unknown exception.");
    }

    curl_global_cleanup();
    return 0;
}
