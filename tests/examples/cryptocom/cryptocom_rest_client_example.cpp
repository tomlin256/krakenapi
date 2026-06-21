// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// CLI demo of every public Crypto.com Exchange v1 REST endpoint via
// CryptoComRestClient. All endpoints here are public — no credentials required.
//
// Usage:
//   cryptocom_rest_client_example instruments
//   cryptocom_rest_client_example tickers      [<instrument>]
//   cryptocom_rest_client_example book         <instrument> [--depth N]
//   cryptocom_rest_client_example candlestick  <instrument> [--timeframe 1m]
//   cryptocom_rest_client_example trades       <instrument> [--count N]
//
// Examples:
//   cryptocom_rest_client_example tickers BTC_USD
//   cryptocom_rest_client_example book BTC_USD --depth 10
//   cryptocom_rest_client_example candlestick BTC_USD --timeframe 5m

#include "exchange/cryptocom/rest_client.hpp"

#include <CLI/CLI.hpp>
#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <string>

using namespace exchange::cryptocom::rest;

namespace {

template<typename Resp>
bool report(const exchange::rest::RestResponse<Resp>& resp) {
    if (!resp.ok)
        spdlog::error("request failed: {}",
                      resp.errors.empty() ? "unknown" : resp.errors.front());
    return resp.ok;
}

} // namespace

int main(int argc, char** argv) {
    curl_global_init(CURL_GLOBAL_ALL);

    CLI::App app{"Crypto.com Exchange public REST demo"};
    app.require_subcommand(1);

    std::string instrument;
    std::string timeframe = "1m";
    int         depth     = 0;
    int         count     = 0;

    app.add_subcommand("instruments", "All tradable instruments");

    auto* tickers = app.add_subcommand("tickers", "Tickers (all, or one instrument)");
    tickers->add_option("instrument", instrument, "e.g. BTC_USD");

    auto* book = app.add_subcommand("book", "Order book");
    book->add_option("instrument", instrument, "e.g. BTC_USD")->required();
    book->add_option("--depth", depth, "Levels per side, e.g. 10");

    auto* candle = app.add_subcommand("candlestick", "OHLC candles");
    candle->add_option("instrument", instrument, "e.g. BTC_USD")->required();
    candle->add_option("--timeframe", timeframe, "e.g. 1m/5m/1h/1D (default 1m)");

    auto* trades = app.add_subcommand("trades", "Recent public trades");
    trades->add_option("instrument", instrument, "e.g. BTC_USD")->required();
    trades->add_option("--count", count, "Max trades to return");

    CLI11_PARSE(app, argc, argv);

    CryptoComRestClient client;
    bool ok = true;

    if (app.got_subcommand("instruments")) {
        auto resp = client.execute(CryptoComInstrumentsRequest{});
        if ((ok = report(resp))) {
            spdlog::info("{} instruments", resp.result->instruments.size());
            for (size_t i = 0; i < resp.result->instruments.size() && i < 5; ++i) {
                const auto& in = resp.result->instruments[i];
                spdlog::info("  {} ({}/{}) type={} tradable={}", in.symbol, in.base_ccy,
                             in.quote_ccy, in.inst_type, in.tradable);
            }
        }

    } else if (*tickers) {
        CryptoComTickersRequest req;
        if (!instrument.empty()) req.instrument_name = instrument;
        auto resp = client.execute(req);
        if ((ok = report(resp))) {
            spdlog::info("{} tickers", resp.result->tickers.size());
            for (size_t i = 0; i < resp.result->tickers.size() && i < 5; ++i) {
                const auto& t = resp.result->tickers[i];
                spdlog::info("  {}: last={:.2f} bid={:.2f} ask={:.2f} vol24h={:.4f}",
                             t.instrument_name, t.last, t.bid, t.ask, t.volume);
            }
        }

    } else if (*book) {
        CryptoComOrderBookRequest req;
        req.instrument_name = instrument;
        if (depth > 0) req.depth = depth;
        auto resp = client.execute(req);
        if ((ok = report(resp))) {
            spdlog::info("{}: depth={} bids={} asks={}", resp.result->instrument_name,
                         resp.result->depth, resp.result->bids.size(),
                         resp.result->asks.size());
            if (!resp.result->bids.empty() && !resp.result->asks.empty())
                spdlog::info("  top bid {:.2f} x {:.8f} | top ask {:.2f} x {:.8f}",
                             resp.result->bids[0].price, resp.result->bids[0].size,
                             resp.result->asks[0].price, resp.result->asks[0].size);
        }

    } else if (*candle) {
        CryptoComCandlesRequest req;
        req.instrument_name = instrument;
        req.timeframe       = timeframe;
        auto resp = client.execute(req);
        if ((ok = report(resp))) {
            spdlog::info("{} {}: {} candles", resp.result->instrument_name,
                         resp.result->interval, resp.result->candles.size());
            if (!resp.result->candles.empty()) {
                const auto& c = resp.result->candles.back();
                spdlog::info("  t={} o={:.2f} h={:.2f} l={:.2f} c={:.2f} v={:.4f}",
                             c.t, c.o, c.h, c.l, c.c, c.v);
            }
        }

    } else if (*trades) {
        CryptoComTradesRequest req;
        req.instrument_name = instrument;
        if (count > 0) req.count = count;
        auto resp = client.execute(req);
        if ((ok = report(resp))) {
            spdlog::info("{} trades", resp.result->trades.size());
            for (size_t i = 0; i < resp.result->trades.size() && i < 5; ++i) {
                const auto& t = resp.result->trades[i];
                spdlog::info("  {} {:.2f} x {:.8f} @ {}", t.side, t.price, t.quantity,
                             t.timestamp);
            }
        }
    }

    curl_global_cleanup();
    return ok ? 0 : 1;
}
