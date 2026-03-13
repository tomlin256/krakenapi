// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include <stdexcept>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "kraken_rest_client.hpp"

using namespace kraken::rest;

int main(int argc, char* argv[])
{
    CLI::App app{"Kraken private REST example — obtain a WebSocket session token"};

    std::string creds_name = "default";
    app.add_option("-c,--credentials", creds_name,
                   "Credentials profile name (file at ~/.kraken/<name>)")
        ->capture_default_str();

    CLI11_PARSE(app, argc, argv);

    curl_global_init(CURL_GLOBAL_ALL);

    try {
        KrakenRestClient client;
        auto creds = Credentials::from_file(creds_name);

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
