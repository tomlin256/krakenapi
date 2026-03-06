// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Demonstrates the three-phase subscription lifecycle using KrakenWsClient.
//
// Usage:
//   ws_client_example <symbol>

#include "kraken_ix_ws_connection.hpp"  // IxWsConnection + make_ws_client(url)

#include <spdlog/spdlog.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────
// Public example: ticker subscription + connection reuse
// ─────────────────────────────────────────────────────────────────────────────

static void run_public_example(const std::string& symbol) {
    spdlog::info("=== Public WebSocket example: {} ===", symbol);

    // Factory creates a fresh IxWsConnection and starts connecting.
    // Phase 1 (on_open) fires asynchronously; subscribe() queues the request
    // internally until the socket is open.
    auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

    kraken::ws::TickerSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{symbol};

    // Phase 2: subscribe request queued (connection may not be open yet).
    // Phase 3: blocks here until the server ack arrives (or times out).
    auto [ack, handle] = client->subscribe(
        sub_req,
        [](const kraken::ws::TickerMessage& msg) {
            for (const auto& d : msg.data)
                spdlog::info("Ticker {}: bid={:.2f} ask={:.2f} last={:.2f}",
                             d.symbol, d.bid, d.ask, d.last);
        },
        std::chrono::milliseconds{10000}
    );

    if (!ack.ok) {
        spdlog::error("Subscription failed: {}", ack.error.value_or("unknown"));
        return;
    }
    spdlog::info("Subscribed to {} – push data flowing", symbol);

    // Receive push data for a while.
    std::this_thread::sleep_for(std::chrono::seconds{10});

    // Unsubscribe.
    handle.cancel();
    spdlog::info("Unsubscribed");

    // ── Connection reuse: wrap an existing IxWsConnection ─────────────────
    // Illustrates make_ws_client(shared_ptr<IWsConnection>) – the second client
    // reuses the same TCP connection without a new handshake.
    spdlog::info("Demonstrating connection reuse…");
    auto raw_conn = std::make_shared<kraken::ws::IxWsConnection>(
                        std::string(kraken::ws::PUBLIC_WS_URL));
    auto client2 = kraken::ws::make_ws_client(
                        std::static_pointer_cast<kraken::ws::IWsConnection>(raw_conn));
    raw_conn->connect();

    kraken::ws::TickerSubscribeRequest sub2;
    sub2.symbols = std::vector<std::string>{symbol};
    auto [ack2, handle2] = client2->subscribe(
        sub2,
        [](const kraken::ws::TickerMessage& msg) {
            for (const auto& d : msg.data)
                spdlog::info("[reused conn] Ticker {}: bid={:.2f}", d.symbol, d.bid);
        },
        std::chrono::milliseconds{10000}
    );
    if (ack2.ok) {
        std::this_thread::sleep_for(std::chrono::seconds{5});
        handle2.cancel();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <symbol>\n";
        return 1;
    }

    try {
        run_public_example(argv[1]);
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
