// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// CLI demo of every public Coinbase Exchange REST endpoint via CoinbaseRestClient.
// All endpoints here are public — no credentials required.
//
// Usage:
//   coinbase_rest_client_example time
//   coinbase_rest_client_example products
//   coinbase_rest_client_example product  <product-id>
//   coinbase_rest_client_example book     <product-id> [--level N]
//   coinbase_rest_client_example ticker   <product-id>
//   coinbase_rest_client_example trades   <product-id> [--limit N]
//   coinbase_rest_client_example candles  <product-id> [--granularity S]
//   coinbase_rest_client_example stats    <product-id>
//
// Examples:
//   coinbase_rest_client_example ticker BTC-USD
//   coinbase_rest_client_example book BTC-USD --level 2
//   coinbase_rest_client_example candles BTC-USD --granularity 3600

#include "exchange/coinbase/rest_client.hpp"

#include <CLI/CLI.hpp>
#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <string>

using namespace exchange::coinbase::rest;

namespace {

// Logs the error list of a failed response. Returns ok so callers can `return
// report(resp)` to forward the success flag as a process exit code.
template<typename Resp>
bool report(const exchange::rest::RestResponse<Resp>& resp) {
    if (!resp.ok) {
        spdlog::error("request failed: {}",
                      resp.errors.empty() ? "unknown" : resp.errors.front());
    }
    return resp.ok;
}

} // namespace

int main(int argc, char** argv) {
    curl_global_init(CURL_GLOBAL_ALL);

    CLI::App app{"Coinbase Exchange public REST demo"};
    app.require_subcommand(1);

    std::string product_id;
    int         level       = 0;
    int         limit       = 0;
    int         granularity = 0;

    app.add_subcommand("time", "Server time");
    app.add_subcommand("products", "All tradable products");

    auto* product = app.add_subcommand("product", "One product");
    product->add_option("product_id", product_id, "e.g. BTC-USD")->required();

    auto* book = app.add_subcommand("book", "Order book");
    book->add_option("product_id", product_id, "e.g. BTC-USD")->required();
    book->add_option("--level", level, "1 (best) or 2 (top 50 aggregated)");

    auto* ticker = app.add_subcommand("ticker", "Best bid/ask + last trade");
    ticker->add_option("product_id", product_id, "e.g. BTC-USD")->required();

    auto* trades = app.add_subcommand("trades", "Recent trades");
    trades->add_option("product_id", product_id, "e.g. BTC-USD")->required();
    trades->add_option("--limit", limit, "Max trades to return");

    auto* candles = app.add_subcommand("candles", "Historic OHLC candles");
    candles->add_option("product_id", product_id, "e.g. BTC-USD")->required();
    candles->add_option("--granularity", granularity,
                        "Seconds: 60/300/900/3600/21600/86400");

    auto* stats = app.add_subcommand("stats", "24h stats");
    stats->add_option("product_id", product_id, "e.g. BTC-USD")->required();

    CLI11_PARSE(app, argc, argv);

    CoinbaseRestClient client;
    bool ok = true;

    if (app.got_subcommand("time")) {
        auto resp = client.execute(CoinbaseServerTimeRequest{});
        if ((ok = report(resp)))
            spdlog::info("server time: {} (epoch {:.3f})", resp.result->iso, resp.result->epoch);

    } else if (app.got_subcommand("products")) {
        auto resp = client.execute(CoinbaseProductsRequest{});
        if ((ok = report(resp))) {
            spdlog::info("{} products", resp.result->products.size());
            for (size_t i = 0; i < resp.result->products.size() && i < 5; ++i) {
                const auto& p = resp.result->products[i];
                spdlog::info("  {} ({}/{}) status={}", p.id, p.base_currency,
                             p.quote_currency, p.status);
            }
        }

    } else if (*product) {
        CoinbaseProductRequest req;
        req.product_id = product_id;
        auto resp = client.execute(req);
        if ((ok = report(resp)))
            spdlog::info("{}: base_inc={} quote_inc={} status={}", resp.result->id,
                         resp.result->base_increment, resp.result->quote_increment,
                         resp.result->status);

    } else if (*book) {
        CoinbaseOrderBookRequest req;
        req.product_id = product_id;
        if (level > 0) req.level = level;
        auto resp = client.execute(req);
        if ((ok = report(resp))) {
            spdlog::info("seq={} bids={} asks={}", resp.result->sequence,
                         resp.result->bids.size(), resp.result->asks.size());
            if (!resp.result->bids.empty() && !resp.result->asks.empty())
                spdlog::info("  top bid {:.2f} x {:.8f} | top ask {:.2f} x {:.8f}",
                             resp.result->bids[0].price, resp.result->bids[0].size,
                             resp.result->asks[0].price, resp.result->asks[0].size);
        }

    } else if (*ticker) {
        CoinbaseTickerRequest req;
        req.product_id = product_id;
        auto resp = client.execute(req);
        if ((ok = report(resp)))
            spdlog::info("{}: price={:.2f} bid={:.2f} ask={:.2f} vol={:.4f} time={}",
                         product_id, resp.result->price, resp.result->bid,
                         resp.result->ask, resp.result->volume, resp.result->time);

    } else if (*trades) {
        CoinbaseTradesRequest req;
        req.product_id = product_id;
        if (limit > 0) req.limit = limit;
        auto resp = client.execute(req);
        if ((ok = report(resp))) {
            spdlog::info("{} trades", resp.result->trades.size());
            for (size_t i = 0; i < resp.result->trades.size() && i < 5; ++i) {
                const auto& t = resp.result->trades[i];
                spdlog::info("  id={} {} {:.2f} x {:.8f} @ {}", t.trade_id, t.side,
                             t.price, t.size, t.time);
            }
        }

    } else if (*candles) {
        CoinbaseCandlesRequest req;
        req.product_id = product_id;
        if (granularity > 0) req.granularity = granularity;
        auto resp = client.execute(req);
        if ((ok = report(resp))) {
            spdlog::info("{} candles", resp.result->candles.size());
            if (!resp.result->candles.empty()) {
                const auto& c = resp.result->candles[0];
                spdlog::info("  t={} o={:.2f} h={:.2f} l={:.2f} c={:.2f} v={:.4f}",
                             c.time, c.open, c.high, c.low, c.close, c.volume);
            }
        }

    } else if (*stats) {
        CoinbaseStatsRequest req;
        req.product_id = product_id;
        auto resp = client.execute(req);
        if ((ok = report(resp)))
            spdlog::info("{}: open={:.2f} high={:.2f} low={:.2f} last={:.2f} vol={:.4f}",
                         product_id, resp.result->open, resp.result->high,
                         resp.result->low, resp.result->last, resp.result->volume);
    }

    curl_global_cleanup();
    return ok ? 0 : 1;
}
