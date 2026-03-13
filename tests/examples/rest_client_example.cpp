// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Demonstrates all public REST endpoints via KrakenRestClient.
//
// Usage:
//   rest_client_example time
//   rest_client_example status
//   rest_client_example assets      [--assets <XBT,ETH,...>]
//   rest_client_example pairs       [--pairs <XBTUSD,ETHUSD,...>]
//   rest_client_example ticker      [--pairs <XBTUSD,ETHUSD,...>]
//   rest_client_example ohlc        <pair> [--interval <N>] [--since <ts>]
//   rest_client_example depth       <pair> [--count <N>]
//   rest_client_example trades      <pair> [--since <ts>] [--count <N>]
//
// Examples:
//   rest_client_example time
//   rest_client_example assets      --assets XBT,ETH
//   rest_client_example pairs       --pairs XBTUSD,ETHUSD
//   rest_client_example ticker      --pairs XXBTZUSD,XETHZUSD
//   rest_client_example ohlc        XXBTZUSD --interval 60
//   rest_client_example depth       XXBTZUSD --count 10
//   rest_client_example trades      XXBTZUSD --count 5

#include "kraken_rest_client.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace kraken::rest;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::optional<std::string> flag_value(const std::vector<std::string>& args,
                                              const std::string& flag) {
    for (std::size_t i = 0; i + 1 < args.size(); ++i)
        if (args[i] == flag)
            return args[i + 1];
    return std::nullopt;
}

// Split a comma-separated string into a vector of tokens.
static std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ','))
        if (!token.empty())
            out.push_back(token);
    return out;
}

static void print_usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " time\n"
        << "  " << prog << " status\n"
        << "  " << prog << " assets   [--assets <XBT,ETH,...>]\n"
        << "  " << prog << " pairs    [--pairs <XBTUSD,ETHUSD,...>]\n"
        << "  " << prog << " ticker   [--pairs <XXBTZUSD,XETHZUSD,...>]\n"
        << "  " << prog << " ohlc     <pair> [--interval <N>] [--since <ts>]\n"
        << "  " << prog << " depth    <pair> [--count <N>]\n"
        << "  " << prog << " trades   <pair> [--since <ts>] [--count <N>]\n"
        << "\n"
        << "Endpoints:\n"
        << "  time      Server time (GET /0/public/Time)\n"
        << "  status    System status (GET /0/public/SystemStatus)\n"
        << "  assets    Asset info; omit --assets for all (GET /0/public/Assets)\n"
        << "  pairs     Trading pair info; omit --pairs for all (GET /0/public/AssetPairs)\n"
        << "  ticker    Level 1 price data; omit --pairs for all (GET /0/public/Ticker)\n"
        << "  ohlc      OHLC candles; interval in minutes: 1|5|15|30|60|240|1440|10080|21600\n"
        << "            (GET /0/public/OHLC)\n"
        << "  depth     Order book; count = 1..500 (GET /0/public/Depth)\n"
        << "  trades    Recent public trades (GET /0/public/Trades)\n"
        << "\n"
        << "Examples:\n"
        << "  " << prog << " time\n"
        << "  " << prog << " assets   --assets XBT,ETH\n"
        << "  " << prog << " pairs    --pairs XBTUSD,ETHUSD\n"
        << "  " << prog << " ticker   --pairs XXBTZUSD,XETHZUSD\n"
        << "  " << prog << " ohlc     XXBTZUSD --interval 60\n"
        << "  " << prog << " depth    XXBTZUSD --count 10\n"
        << "  " << prog << " trades   XXBTZUSD --count 5\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// time
// ─────────────────────────────────────────────────────────────────────────────

static void run_time(KrakenRestClient& client) {
    spdlog::info("=== Server Time ===");

    auto resp = client.execute(GetServerTimeRequest{});
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    const auto& t = *resp.result;
    spdlog::info("  unixtime : {}", t.unixtime);
    spdlog::info("  rfc1123  : {}", t.rfc1123);
}

// ─────────────────────────────────────────────────────────────────────────────
// status
// ─────────────────────────────────────────────────────────────────────────────

static void run_status(KrakenRestClient& client) {
    spdlog::info("=== System Status ===");

    auto resp = client.execute(GetSystemStatusRequest{});
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    const auto& s = *resp.result;
    spdlog::info("  status    : {}", s.status);
    spdlog::info("  timestamp : {}", s.timestamp);
}

// ─────────────────────────────────────────────────────────────────────────────
// assets
// ─────────────────────────────────────────────────────────────────────────────

static void run_assets(KrakenRestClient& client,
                       const std::optional<std::vector<std::string>>& filter) {
    if (filter)
        spdlog::info("=== Asset Info ({}) ===",
                     [&]{ std::string s; for (auto& a : *filter) { if (!s.empty()) s+=','; s+=a; } return s; }());
    else
        spdlog::info("=== Asset Info (all) ===");

    GetAssetInfoRequest req;
    req.assets = filter;

    auto resp = client.execute(req);
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    spdlog::info("  {} asset(s) returned", resp.result->assets.size());
    for (const auto& [name, info] : resp.result->assets)
        spdlog::info("  {:>8}  altname={:<8}  class={:<10}  decimals={}/{}",
                     name, info.altname, info.aclass,
                     info.display_decimals, info.decimals);
}

// ─────────────────────────────────────────────────────────────────────────────
// pairs
// ─────────────────────────────────────────────────────────────────────────────

static void run_pairs(KrakenRestClient& client,
                      const std::optional<std::vector<std::string>>& filter) {
    if (filter)
        spdlog::info("=== Asset Pairs ({}) ===",
                     [&]{ std::string s; for (auto& p : *filter) { if (!s.empty()) s+=','; s+=p; } return s; }());
    else
        spdlog::info("=== Asset Pairs (all) ===");

    GetAssetPairsRequest req;
    req.pairs = filter;

    auto resp = client.execute(req);
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    spdlog::info("  {} pair(s) returned", resp.result->pairs.size());
    for (const auto& [name, info] : resp.result->pairs)
        spdlog::info("  {:>12}  wsname={:<12}  base={:<5}  quote={:<5}"
                     "  pair_dec={}  lot_dec={}  ordermin={:.8f}",
                     name, info.wsname, info.base, info.quote,
                     info.pair_decimals, info.lot_decimals, info.ordermin);
}

// ─────────────────────────────────────────────────────────────────────────────
// ticker
// ─────────────────────────────────────────────────────────────────────────────

static void run_ticker(KrakenRestClient& client,
                       const std::optional<std::vector<std::string>>& filter) {
    if (filter)
        spdlog::info("=== Ticker ({}) ===",
                     [&]{ std::string s; for (auto& p : *filter) { if (!s.empty()) s+=','; s+=p; } return s; }());
    else
        spdlog::info("=== Ticker (all) ===");

    GetTickerRequest req;
    req.pairs = filter;

    auto resp = client.execute(req);
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    spdlog::info("  {} ticker(s) returned", resp.result->tickers.size());
    for (const auto& [pair, t] : resp.result->tickers)
        spdlog::info("  {:>12}  bid={:.4f}  ask={:.4f}  last={:.4f}"
                     "  vol_24h={:.2f}  vwap_24h={:.4f}"
                     "  low_24h={:.4f}  high_24h={:.4f}  open={:.4f}",
                     pair, t.bid, t.ask, t.last,
                     t.volume_24h, t.vwap_24h,
                     t.low_24h, t.high_24h, t.open);
}

// ─────────────────────────────────────────────────────────────────────────────
// ohlc
// ─────────────────────────────────────────────────────────────────────────────

static void run_ohlc(KrakenRestClient& client,
                     const std::string& pair,
                     std::optional<int32_t> interval,
                     std::optional<int64_t> since) {
    spdlog::info("=== OHLC: {} interval={}m ===",
                 pair, interval.value_or(1));

    GetOHLCRequest req;
    req.pair     = pair;
    req.interval = interval;
    req.since    = since;

    auto resp = client.execute(req);
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    const auto& r = *resp.result;
    spdlog::info("  pair={} candles={} last={}",
                 r.pair, r.candles.size(), r.last);

    // Print the 5 most recent candles.
    std::size_t start = r.candles.size() > 5 ? r.candles.size() - 5 : 0;
    for (std::size_t i = start; i < r.candles.size(); ++i) {
        const auto& c = r.candles[i];
        spdlog::info("  ts={} O={:.4f} H={:.4f} L={:.4f} C={:.4f}"
                     " vwap={:.4f} vol={:.4f} trades={}",
                     c.time, c.open, c.high, c.low, c.close,
                     c.vwap, c.volume, c.count);
    }
    if (start > 0)
        spdlog::info("  (showing last 5 of {} candles)", r.candles.size());
}

// ─────────────────────────────────────────────────────────────────────────────
// depth
// ─────────────────────────────────────────────────────────────────────────────

static void run_depth(KrakenRestClient& client,
                      const std::string& pair,
                      std::optional<int32_t> count) {
    spdlog::info("=== Order Book: {} count={} ===",
                 pair, count.value_or(10));

    GetOrderBookRequest req;
    req.pair  = pair;
    req.count = count;

    auto resp = client.execute(req);
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    const auto& r = *resp.result;
    spdlog::info("  pair={}  asks={}  bids={}", r.pair, r.asks.size(), r.bids.size());

    // Print top 5 asks and bids.
    std::size_t n = std::min<std::size_t>(5, r.asks.size());
    spdlog::info("  --- Top {} asks ---", n);
    for (std::size_t i = 0; i < n; ++i)
        spdlog::info("    ask[{}]  price={:.4f}  vol={:.6f}  ts={}",
                     i, r.asks[i].price, r.asks[i].volume, r.asks[i].timestamp);

    n = std::min<std::size_t>(5, r.bids.size());
    spdlog::info("  --- Top {} bids ---", n);
    for (std::size_t i = 0; i < n; ++i)
        spdlog::info("    bid[{}]  price={:.4f}  vol={:.6f}  ts={}",
                     i, r.bids[i].price, r.bids[i].volume, r.bids[i].timestamp);
}

// ─────────────────────────────────────────────────────────────────────────────
// trades
// ─────────────────────────────────────────────────────────────────────────────

static void run_trades(KrakenRestClient& client,
                       const std::string& pair,
                       std::optional<int64_t> since,
                       std::optional<int32_t> count) {
    spdlog::info("=== Recent Trades: {} ===", pair);

    GetRecentTradesRequest req;
    req.pair  = pair;
    req.since = since;
    req.count = count;

    auto resp = client.execute(req);
    if (!resp.ok || !resp.result) {
        for (const auto& e : resp.errors)
            spdlog::error("  error: {}", e);
        return;
    }

    const auto& r = *resp.result;
    spdlog::info("  pair={}  trades={}  last={}", r.pair, r.trades.size(), r.last);

    for (const auto& t : r.trades)
        spdlog::info("  price={:.4f}  vol={:.6f}  time={:.3f}  side={}  type={}",
                     t.price, t.volume, t.time,
                     t.side == kraken::Side::Buy ? "buy" : "sell",
                     t.order_type == "l" ? "limit" : "market");
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string cmd = argv[1];

    // Collect remaining arguments for option parsing.
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i)
        args.emplace_back(argv[i]);

    // First positional extra arg (if not starting with "--") is the pair.
    std::string pair_arg;
    if (!args.empty() && args[0].rfind("--", 0) != 0)
        pair_arg = args[0];

    curl_global_init(CURL_GLOBAL_ALL);
    KrakenRestClient client;

    try {
        if (cmd == "time") {
            run_time(client);

        } else if (cmd == "status") {
            run_status(client);

        } else if (cmd == "assets") {
            std::optional<std::vector<std::string>> filter;
            if (auto v = flag_value(args, "--assets"))
                filter = split_csv(*v);
            run_assets(client, filter);

        } else if (cmd == "pairs") {
            std::optional<std::vector<std::string>> filter;
            if (auto v = flag_value(args, "--pairs"))
                filter = split_csv(*v);
            run_pairs(client, filter);

        } else if (cmd == "ticker") {
            std::optional<std::vector<std::string>> filter;
            if (auto v = flag_value(args, "--pairs"))
                filter = split_csv(*v);
            run_ticker(client, filter);

        } else if (cmd == "ohlc") {
            if (pair_arg.empty()) {
                std::cerr << "Error: 'ohlc' requires a <pair> argument.\n\n";
                print_usage(argv[0]);
                curl_global_cleanup();
                return 1;
            }
            std::optional<int32_t> interval;
            std::optional<int64_t> since;
            if (auto v = flag_value(args, "--interval"))
                interval = std::stoi(*v);
            if (auto v = flag_value(args, "--since"))
                since = std::stoll(*v);
            run_ohlc(client, pair_arg, interval, since);

        } else if (cmd == "depth") {
            if (pair_arg.empty()) {
                std::cerr << "Error: 'depth' requires a <pair> argument.\n\n";
                print_usage(argv[0]);
                curl_global_cleanup();
                return 1;
            }
            std::optional<int32_t> count;
            if (auto v = flag_value(args, "--count"))
                count = std::stoi(*v);
            run_depth(client, pair_arg, count);

        } else if (cmd == "trades") {
            if (pair_arg.empty()) {
                std::cerr << "Error: 'trades' requires a <pair> argument.\n\n";
                print_usage(argv[0]);
                curl_global_cleanup();
                return 1;
            }
            std::optional<int64_t> since;
            std::optional<int32_t> count;
            if (auto v = flag_value(args, "--since"))
                since = std::stoll(*v);
            if (auto v = flag_value(args, "--count"))
                count = std::stoi(*v);
            run_trades(client, pair_arg, since, count);

        } else {
            std::cerr << "Error: unknown command '" << cmd << "'.\n\n";
            print_usage(argv[0]);
            curl_global_cleanup();
            return 1;
        }
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
