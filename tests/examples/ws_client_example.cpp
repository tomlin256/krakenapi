// ws_client_example.cpp
//
// Demonstrates the three-phase subscription lifecycle and sync/async execute
// patterns using KrakenWsClient.
//
// Usage:
//   ws_client_example <symbol>              – public ticker subscription
//   ws_client_example <symbol> <key> <b64-secret>  – also places a validate-only order

#include "kraken_ix_ws_connection.hpp"  // IxWsConnection + make_ws_client(url)
#include "kraken_rest_client.hpp"       // for GetWebSocketsTokenRequest

#include <spdlog/spdlog.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static std::string get_ws_token(const std::string& api_key,
                                const std::string& api_secret) {
    kraken::rest::KrakenRestClient rest;
    kraken::rest::Credentials creds{api_key, api_secret};
    auto resp = rest.execute(kraken::rest::GetWebSocketsTokenRequest{}, creds);
    if (!resp.ok || !resp.result)
        throw std::runtime_error("Failed to get WS token");
    return resp.result->token;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public example: ticker subscription
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
}

// ─────────────────────────────────────────────────────────────────────────────
// Private example: place a validate-only order (sync), then cancel-all (async)
// ─────────────────────────────────────────────────────────────────────────────

static void run_private_example(const std::string& symbol,
                                const std::string& api_key,
                                const std::string& api_secret) {
    spdlog::info("=== Private WebSocket example ===");

    const std::string token = get_ws_token(api_key, api_secret);

    auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PRIVATE_WS_URL));

    // ── Sync execute: validate-only limit order ────────────────────────────
    kraken::ws::AddOrderRequest order;
    order.token       = token;
    order.order_type  = kraken::OrderType::Limit;
    order.side        = kraken::Side::Buy;
    order.symbol      = symbol;
    order.order_qty   = 0.0001;
    order.limit_price = 1.0;
    order.validate    = true;

    spdlog::info("Placing validate-only limit buy…");
    auto resp = client->execute(order, std::chrono::milliseconds{10000});

    if (resp.ok && resp.result)
        spdlog::info("Order validated – order_id: {}",
                     resp.result->order_id.value_or("(none)"));
    else
        spdlog::warn("Order failed: {}", resp.error.value_or("unknown"));

    // ── Async execute: cancel-all ──────────────────────────────────────────
    kraken::ws::CancelAllRequest cancel_req;
    cancel_req.token = token;

    spdlog::info("Sending cancel_all asynchronously…");
    auto fut = client->execute_async(cancel_req);

    // Do other work while waiting…

    auto cancel_resp = fut.get();
    if (cancel_resp.ok && cancel_resp.result)
        spdlog::info("cancel_all: {} order(s) cancelled",
                     cancel_resp.result->count.value_or(0));
    else
        spdlog::warn("cancel_all failed: {}", cancel_resp.error.value_or("unknown"));

    // ── Connection reuse: wrap an existing IxWsConnection ─────────────────
    // (Illustrates the make_ws_client(shared_ptr<IWsConnection>) overload.)
    spdlog::info("Demonstrating connection reuse…");
    auto raw_conn = std::make_shared<kraken::ws::IxWsConnection>(
                        std::string(kraken::ws::PUBLIC_WS_URL));
    auto client2 = kraken::ws::make_ws_client(
                        std::static_pointer_cast<kraken::ws::IWsConnection>(raw_conn));
    raw_conn->connect();

    kraken::ws::TickerSubscribeRequest pub_sub;
    pub_sub.symbols = std::vector<std::string>{symbol};
    auto [pub_ack, pub_handle] = client2->subscribe(
        pub_sub,
        [](const kraken::ws::TickerMessage& msg) {
            for (const auto& d : msg.data)
                spdlog::info("[reused conn] Ticker {}: bid={:.2f}", d.symbol, d.bid);
        },
        std::chrono::milliseconds{10000}
    );
    if (pub_ack.ok) {
        std::this_thread::sleep_for(std::chrono::seconds{5});
        pub_handle.cancel();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <symbol> [<api_key> <api_secret_base64>]\n";
        return 1;
    }

    const std::string symbol = argv[1];

    try {
        run_public_example(symbol);

        if (argc == 4)
            run_private_example(symbol, argv[2], argv[3]);
    } catch (const std::exception& e) {
        spdlog::error("Fatal: {}", e.what());
        return 1;
    }

    return 0;
}
