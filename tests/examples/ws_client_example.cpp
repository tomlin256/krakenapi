// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Demonstrates all public WebSocket subscription channels via KrakenWsClient.
//
// Usage:
//   ws_client_example ticker     <symbol>
//   ws_client_example book       <symbol> [--depth <N>]
//   ws_client_example trade      <symbol>
//   ws_client_example ohlc       <symbol> [--interval <N>]
//   ws_client_example instrument
//
// Examples:
//   ws_client_example ticker     BTC/USD
//   ws_client_example book       BTC/USD --depth 10
//   ws_client_example trade      ETH/USD
//   ws_client_example ohlc       BTC/USD --interval 5
//   ws_client_example instrument

#include "kraken_ix_ws_connection.hpp"  // IxWsConnection + make_ws_client(url)

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

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

static void print_usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " ticker     <symbol>\n"
        << "  " << prog << " book       <symbol> [--depth <N>]\n"
        << "  " << prog << " trade      <symbol>\n"
        << "  " << prog << " ohlc       <symbol> [--interval <N>]\n"
        << "  " << prog << " instrument\n"
        << "\n"
        << "Channels:\n"
        << "  ticker      Level 1 best bid/ask, last price, 24h stats\n"
        << "  book        Level 2 order book; depth = 10 | 25 | 100 | 500 | 1000 (default 10)\n"
        << "  trade       Public trades feed\n"
        << "  ohlc        OHLC candles; interval in minutes: 1 | 5 | 15 | 30 | 60 |\n"
        << "              240 | 1440 | 10080 | 21600 (default 1)\n"
        << "  instrument  Static instrument/market info (no symbol required)\n"
        << "\n"
        << "Examples:\n"
        << "  " << prog << " ticker     BTC/USD\n"
        << "  " << prog << " book       BTC/USD --depth 10\n"
        << "  " << prog << " trade      ETH/USD\n"
        << "  " << prog << " ohlc       BTC/USD --interval 5\n"
        << "  " << prog << " instrument\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// ticker
// ─────────────────────────────────────────────────────────────────────────────

static void run_ticker(const std::string& symbol) {
    spdlog::info("=== Ticker subscription: {} ===", symbol);

    auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

    kraken::ws::TickerSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{symbol};

    auto [ack, handle] = client->subscribe(
        sub_req,
        [](const kraken::ws::TickerMessage& msg) {
            for (const auto& d : msg.data)
                spdlog::info("[ticker] {} bid={:.4f} ask={:.4f} last={:.4f} "
                             "vol={:.4f} chg={:+.2f}%",
                             d.symbol, d.bid, d.ask, d.last,
                             d.volume, d.change_pct);
        },
        std::chrono::milliseconds{10000}
    );

    if (!ack.ok) {
        spdlog::error("Subscription failed: {}", ack.error.value_or("unknown"));
        return;
    }
    spdlog::info("Subscribed — receiving push data for 10 s");
    std::this_thread::sleep_for(std::chrono::seconds{10});
    handle.cancel();
    spdlog::info("Unsubscribed");

    // ── Connection reuse demo ─────────────────────────────────────────────────
    spdlog::info("Demonstrating connection reuse with a second KrakenWsClient…");
    auto raw_conn = std::make_shared<kraken::ws::IxWsConnection>(
                        std::string(kraken::ws::PUBLIC_WS_URL));
    auto client2  = kraken::ws::make_ws_client(
                        std::static_pointer_cast<kraken::ws::IWsConnection>(raw_conn));
    raw_conn->connect();

    kraken::ws::TickerSubscribeRequest sub2;
    sub2.symbols = std::vector<std::string>{symbol};
    auto [ack2, handle2] = client2->subscribe(
        sub2,
        [](const kraken::ws::TickerMessage& msg) {
            for (const auto& d : msg.data)
                spdlog::info("[ticker/reused] {} bid={:.4f}", d.symbol, d.bid);
        },
        std::chrono::milliseconds{10000}
    );
    if (ack2.ok) {
        std::this_thread::sleep_for(std::chrono::seconds{5});
        handle2.cancel();
        spdlog::info("Reused-connection subscription cancelled");
    } else {
        spdlog::error("Reused-connection subscription failed: {}",
                      ack2.error.value_or("unknown"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// book
// ─────────────────────────────────────────────────────────────────────────────

static void run_book(const std::string& symbol, std::optional<int> depth) {
    spdlog::info("=== Book subscription: {} depth={} ===",
                 symbol, depth.value_or(10));

    auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

    kraken::ws::BookSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{symbol};
    if (depth) sub_req.depth = *depth;

    auto [ack, handle] = client->subscribe(
        sub_req,
        [](const kraken::ws::BookMessage& msg) {
            for (const auto& d : msg.data) {
                spdlog::info("[book/{}] {} bids={} asks={}",
                             msg.type, d.symbol,
                             d.bids.size(), d.asks.size());
                // Print top 3 levels on snapshots to keep output readable.
                if (msg.type == "snapshot") {
                    std::size_t n = std::min<std::size_t>(3, d.bids.size());
                    for (std::size_t i = 0; i < n; ++i)
                        spdlog::info("  bid[{}] price={:.4f} qty={:.6f}",
                                     i, d.bids[i].price, d.bids[i].qty);
                    n = std::min<std::size_t>(3, d.asks.size());
                    for (std::size_t i = 0; i < n; ++i)
                        spdlog::info("  ask[{}] price={:.4f} qty={:.6f}",
                                     i, d.asks[i].price, d.asks[i].qty);
                }
            }
        },
        std::chrono::milliseconds{10000}
    );

    if (!ack.ok) {
        spdlog::error("Subscription failed: {}", ack.error.value_or("unknown"));
        return;
    }
    spdlog::info("Subscribed — receiving push data for 10 s");
    std::this_thread::sleep_for(std::chrono::seconds{10});
    handle.cancel();
    spdlog::info("Unsubscribed");
}

// ─────────────────────────────────────────────────────────────────────────────
// trade
// ─────────────────────────────────────────────────────────────────────────────

static void run_trade(const std::string& symbol) {
    spdlog::info("=== Trade subscription: {} ===", symbol);

    auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

    kraken::ws::TradeSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{symbol};

    auto [ack, handle] = client->subscribe(
        sub_req,
        [](const kraken::ws::TradeMessage& msg) {
            for (const auto& d : msg.data)
                spdlog::info("[trade] {} price={:.4f} qty={:.6f} side={} type={} id={}",
                             d.symbol, d.price, d.qty,
                             d.side, d.ord_type, d.trade_id);
        },
        std::chrono::milliseconds{10000}
    );

    if (!ack.ok) {
        spdlog::error("Subscription failed: {}", ack.error.value_or("unknown"));
        return;
    }
    spdlog::info("Subscribed — receiving push data for 10 s");
    std::this_thread::sleep_for(std::chrono::seconds{10});
    handle.cancel();
    spdlog::info("Unsubscribed");
}

// ─────────────────────────────────────────────────────────────────────────────
// ohlc
// ─────────────────────────────────────────────────────────────────────────────

static void run_ohlc(const std::string& symbol, std::optional<int> interval) {
    spdlog::info("=== OHLC subscription: {} interval={}m ===",
                 symbol, interval.value_or(1));

    auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

    kraken::ws::OHLCSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{symbol};
    if (interval) sub_req.interval = *interval;

    auto [ack, handle] = client->subscribe(
        sub_req,
        [](const kraken::ws::OHLCMessage& msg) {
            for (const auto& d : msg.data)
                spdlog::info("[ohlc/{}] {} ts={} O={:.4f} H={:.4f} L={:.4f} "
                             "C={:.4f} vol={:.4f} trades={}",
                             msg.type, d.symbol, d.timestamp,
                             d.open, d.high, d.low, d.close,
                             d.volume, d.trades);
        },
        std::chrono::milliseconds{10000}
    );

    if (!ack.ok) {
        spdlog::error("Subscription failed: {}", ack.error.value_or("unknown"));
        return;
    }
    spdlog::info("Subscribed — receiving push data for 10 s");
    std::this_thread::sleep_for(std::chrono::seconds{10});
    handle.cancel();
    spdlog::info("Unsubscribed");
}

// ─────────────────────────────────────────────────────────────────────────────
// instrument
// ─────────────────────────────────────────────────────────────────────────────

static void run_instrument() {
    spdlog::info("=== Instrument subscription ===");

    auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

    kraken::ws::InstrumentSubscribeRequest sub_req;
    // No symbols field for this channel — server sends all instruments.

    auto [ack, handle] = client->subscribe(
        sub_req,
        [](const kraken::ws::InstrumentMessage& msg) {
            spdlog::info("[instrument/{}] {} instrument(s)", msg.type, msg.data.size());
            // Print the first few to avoid flooding the terminal.
            std::size_t n = std::min<std::size_t>(5, msg.data.size());
            for (std::size_t i = 0; i < n; ++i) {
                const auto& d = msg.data[i];
                spdlog::info("  {} ({}/{}) status={} qty_min={:.8f} price_inc={:.8f}",
                             d.symbol, d.base, d.quote,
                             d.status, d.qty_min, d.price_increment);
            }
            if (msg.data.size() > n)
                spdlog::info("  … and {} more", msg.data.size() - n);
        },
        std::chrono::milliseconds{10000}
    );

    if (!ack.ok) {
        spdlog::error("Subscription failed: {}", ack.error.value_or("unknown"));
        return;
    }
    spdlog::info("Subscribed — receiving push data for 10 s");
    std::this_thread::sleep_for(std::chrono::seconds{10});
    handle.cancel();
    spdlog::info("Unsubscribed");
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string channel = argv[1];

    // Collect remaining arguments for option parsing.
    std::vector<std::string> rest_args;
    for (int i = 2; i < argc; ++i)
        rest_args.emplace_back(argv[i]);

    // First positional extra arg (if not starting with "--") is the symbol.
    std::string symbol;
    if (!rest_args.empty() && rest_args[0].rfind("--", 0) != 0)
        symbol = rest_args[0];

    try {
        if (channel == "ticker") {
            if (symbol.empty()) {
                std::cerr << "Error: 'ticker' requires a <symbol> argument.\n\n";
                print_usage(argv[0]);
                return 1;
            }
            run_ticker(symbol);

        } else if (channel == "book") {
            if (symbol.empty()) {
                std::cerr << "Error: 'book' requires a <symbol> argument.\n\n";
                print_usage(argv[0]);
                return 1;
            }
            std::optional<int> depth;
            if (auto v = flag_value(rest_args, "--depth"))
                depth = std::stoi(*v);
            run_book(symbol, depth);

        } else if (channel == "trade") {
            if (symbol.empty()) {
                std::cerr << "Error: 'trade' requires a <symbol> argument.\n\n";
                print_usage(argv[0]);
                return 1;
            }
            run_trade(symbol);

        } else if (channel == "ohlc") {
            if (symbol.empty()) {
                std::cerr << "Error: 'ohlc' requires a <symbol> argument.\n\n";
                print_usage(argv[0]);
                return 1;
            }
            std::optional<int> interval;
            if (auto v = flag_value(rest_args, "--interval"))
                interval = std::stoi(*v);
            run_ohlc(symbol, interval);

        } else if (channel == "instrument") {
            run_instrument();

        } else {
            std::cerr << "Error: unknown channel '" << channel << "'.\n\n";
            print_usage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
