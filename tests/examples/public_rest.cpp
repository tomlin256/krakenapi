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
    CLI::App app{"Kraken public REST example — fetch recent trades"};

    std::string pair = "XXBTZEUR";
    app.add_option("-p,--pair", pair, "Trading pair to fetch recent trades for")
        ->capture_default_str();

    CLI11_PARSE(app, argc, argv);

    curl_global_init(CURL_GLOBAL_ALL);

    try {
        KrakenRestClient client;

        GetRecentTradesRequest req;
        req.pair = pair;

        auto resp = client.execute(req);
        if (resp.ok && resp.result) {
            spdlog::info("pair: {}", resp.result->pair);
            spdlog::info("last: {}", resp.result->last);
            spdlog::info("trades ({}):", resp.result->trades.size());
            for (const auto& t : resp.result->trades)
                spdlog::info("  price={} volume={} side={}", t.price, t.volume,
                             t.side == kraken::Side::Buy ? "buy" : "sell");
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
