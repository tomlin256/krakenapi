// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/rest_api.hpp"
#include "binance_rest_example_json.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace exchange::binance::rest;
using json = nlohmann::json;
namespace fixtures = exchange::binance::rest::test;

// ---------------------------------------------------------------------------
// from_json — response parsing
// ---------------------------------------------------------------------------

TEST(BinanceRestResponses, Ping_FromJson_DoesNotThrow) {
    auto j = json::parse(fixtures::kPingJson);
    EXPECT_NO_THROW(BinancePing::from_json(j));
}

TEST(BinanceRestResponses, ServerTime_FromJson) {
    auto j = json::parse(fixtures::kServerTimeJson);
    auto t = BinanceServerTime::from_json(j);
    EXPECT_EQ(t.server_time, 1499827319559LL);
}

TEST(BinanceRestResponses, TickerPrice_SingleObject) {
    auto j = json::parse(fixtures::kTickerPriceSingleJson);
    auto t = BinanceTickerPrice::from_json(j);
    ASSERT_EQ(t.entries.size(), 1u);
    EXPECT_EQ(t.entries[0].symbol, "LTCBTC");
    EXPECT_DOUBLE_EQ(t.entries[0].price, 4.00000200);
}

TEST(BinanceRestResponses, TickerPrice_Array) {
    auto j = json::parse(fixtures::kTickerPriceArrayJson);
    auto t = BinanceTickerPrice::from_json(j);
    ASSERT_EQ(t.entries.size(), 2u);
    EXPECT_EQ(t.entries[0].symbol, "LTCBTC");
    EXPECT_DOUBLE_EQ(t.entries[0].price, 4.00000200);
    EXPECT_EQ(t.entries[1].symbol, "ETHBTC");
    EXPECT_DOUBLE_EQ(t.entries[1].price, 0.07734100);
}

TEST(BinanceRestResponses, OrderBook_FromJson) {
    auto j = json::parse(fixtures::kDepthJson);
    auto b = BinanceOrderBook::from_json(j);
    EXPECT_EQ(b.last_update_id, 1027024LL);
    ASSERT_EQ(b.bids.size(), 1u);
    EXPECT_DOUBLE_EQ(b.bids[0].price, 4.0);
    EXPECT_DOUBLE_EQ(b.bids[0].quantity, 431.0);
    ASSERT_EQ(b.asks.size(), 1u);
    EXPECT_DOUBLE_EQ(b.asks[0].price, 4.000002);
    EXPECT_DOUBLE_EQ(b.asks[0].quantity, 12.0);
}

TEST(BinanceRestResponses, Trades_FromJson) {
    auto j = json::parse(fixtures::kTradesJson);
    auto r = BinanceTradesResult::from_json(j);
    ASSERT_EQ(r.trades.size(), 1u);
    const auto& t = r.trades[0];
    EXPECT_EQ(t.id, 28457LL);
    EXPECT_DOUBLE_EQ(t.price, 4.00000100);
    EXPECT_DOUBLE_EQ(t.qty, 12.0);
    EXPECT_DOUBLE_EQ(t.quote_qty, 48.000012);
    EXPECT_EQ(t.time, 1499865549590LL);
    EXPECT_TRUE(t.is_buyer_maker);
    EXPECT_TRUE(t.is_best_match);
}

TEST(BinanceRestResponses, Trades_EmptyArray) {
    auto r = BinanceTradesResult::from_json(json::array());
    EXPECT_TRUE(r.trades.empty());
}

// ---------------------------------------------------------------------------
// parse_binance_response envelope
// ---------------------------------------------------------------------------

TEST(ParseBinanceResponse, OkResponseSetsOkTrue) {
    auto j = json::parse(fixtures::kPingJson);
    auto resp = parse_binance_response<BinancePing>(200, j);
    EXPECT_TRUE(resp.ok);
    EXPECT_TRUE(resp.errors.empty());
    ASSERT_TRUE(resp.result.has_value());
}

TEST(ParseBinanceResponse, ErrorResponseSetsOkFalse) {
    auto j = json::parse(fixtures::kErrorJson);
    auto resp = parse_binance_response<BinancePing>(400, j);
    EXPECT_FALSE(resp.ok);
    ASSERT_EQ(resp.errors.size(), 1u);
    EXPECT_EQ(resp.errors[0], "Invalid symbol.");
    EXPECT_FALSE(resp.result.has_value());
}
