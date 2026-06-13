// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Compile-proof + behavioural transparency test for the deprecated kraken::
// compatibility shim (plan 002 Phase 3). Pre-refactor callers reached the
// library through the old-path kraken_*.hpp headers and the kraken:: namespace;
// this test guarantees that contract keeps holding through the new
// exchange::kraken:: layout.
//
//  - Including every old-path forwarder below *is* the mechanical compile-proof:
//    if any forwarder (or the kraken_compat.hpp aliases it pulls in) stops
//    resolving against the new layout — e.g. a type moves namespaces without a
//    re-export, as TickPrice did — this file fails to compile.
//  - The static_asserts prove the kraken:: names ARE the new types (correct
//    forwarding, not just "something compiles").
//  - The TESTs prove the forwarded surface still behaves identically.
//
// Built only when KRAKENAPI_BUILD_COMPAT_SHIM is ON, with
// KRAKENAPI_SUPPRESS_DEPRECATION so the forwarders' #pragma message stays quiet.

// Every old-path forwarder header — these includes are the compile-proof.
#include "kraken_types.hpp"
#include "kraken_rest_api.hpp"
#include "kraken_rest_client.hpp"
#include "kraken_ws_api.hpp"
#include "kraken_ws_client.hpp"
#include "kraken_ix_ws_connection.hpp"
#include "ws_reconnect_session.hpp"

#include "mock_ws_connection.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>

// ── Forwarding identity: kraken:: names resolve to the new types ─────────────

static_assert(std::is_same_v<kraken::Side, exchange::Side>);
static_assert(std::is_same_v<kraken::OrderType, exchange::OrderType>);
static_assert(std::is_same_v<kraken::TickPrice, exchange::TickPrice>);          // moved in plan 009
static_assert(std::is_same_v<kraken::OrderParams, exchange::kraken::OrderParams>);
static_assert(std::is_same_v<kraken::rest::KrakenRestClient,
                             exchange::kraken::rest::KrakenRestClient>);
static_assert(std::is_same_v<kraken::ws::KrakenWsClient,
                             exchange::ws::ExchangeWsClient>);                   // alias, not subclass
static_assert(std::is_same_v<kraken::ws::WsReconnectSession,
                             exchange::ws::WsReconnectSession>);

// ── OrderType wire format: the shim must keep Kraken's hyphenated form ────────

TEST(CompatShim, OrderTypeKeepsKrakenWireFormat) {
    // The deliberate, load-bearing shim behaviour (see kraken_compat.hpp's
    // converter comment): kraken::to_string(OrderType) yields Kraken's
    // hyphenated wire string, NOT the generic underscore form ("stop_loss").
    EXPECT_EQ(kraken::to_string(kraken::OrderType::StopLoss), "stop-loss");
    EXPECT_EQ(kraken::order_type_from_string("stop-loss"), kraken::OrderType::StopLoss);
}

// ── Exact-decimal price through the old kraken::TickPrice name ────────────────

TEST(CompatShim, TickPriceRoundTrips) {
    EXPECT_EQ(kraken::TickPrice::from(309.6217, 4).str(), "309.6217");
}

// ── REST round-trip through kraken::rest:: + the old make_test_client ─────────

TEST(CompatShim, RestRoundTripThroughShim) {
    auto client = kraken::rest::make_test_client(
        [](const kraken::rest::HttpRequest& http) {
            EXPECT_EQ(http.path, "/0/public/Time");
            return R"({"error":[],"result":{"unixtime":1700000000,)"
                   R"("rfc1123":"Mon, 14 Nov 2023 00:00:00 +0000"}})";
        });

    const auto resp = client.execute(kraken::rest::GetServerTimeRequest{});
    ASSERT_TRUE(resp.ok);
    ASSERT_TRUE(resp.result.has_value());
    EXPECT_EQ(resp.result->unixtime, 1700000000);
}

// ── WS client built through the deprecated make_ws_client(conn) forwarder ─────

TEST(CompatShim, WsClientThroughDeprecatedForwarder) {
    auto conn   = std::make_shared<MockWsConnection>();
    auto client = kraken::ws::make_ws_client(conn);  // [[deprecated]] conn forwarder
    ASSERT_NE(client, nullptr);

    conn->fire_open();
    client->execute_async(kraken::ws::PingRequest{});

    ASSERT_FALSE(conn->sent_messages.empty());
    EXPECT_NE(conn->sent_messages.back().find("ping"), std::string::npos);
}
