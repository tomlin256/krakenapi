// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Verifies from_json deserialization of every Coinbase REST response type
// against captured fixtures (coinbase_rest_example_json.hpp).

#include "exchange/coinbase/rest_api.hpp"
#include "coinbase_rest_example_json.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

using namespace exchange::coinbase::rest;
using json = nlohmann::json;
namespace fx = coinbase_fixtures;

// ── Public responses ──────────────────────────────────────────────────────────

TEST(CoinbaseRestResponses, ServerTime) {
    auto t = CoinbaseServerTime::from_json(json::parse(fx::TIME_JSON));
    EXPECT_EQ(t.iso, "2026-06-19T16:43:33.188Z");
    EXPECT_DOUBLE_EQ(t.epoch, 1781887413.188);
}

TEST(CoinbaseRestResponses, SingleProduct) {
    auto p = CoinbaseProduct::from_json(json::parse(fx::PRODUCT_JSON));
    EXPECT_EQ(p.id, "BTC-USD");
    EXPECT_EQ(p.base_currency, "BTC");
    EXPECT_EQ(p.quote_currency, "USD");
    EXPECT_DOUBLE_EQ(p.base_increment, 0.00000001);
    EXPECT_DOUBLE_EQ(p.quote_increment, 0.01);
    EXPECT_EQ(p.status, "online");
    EXPECT_FALSE(p.trading_disabled);
}

TEST(CoinbaseRestResponses, ProductsArray) {
    auto r = CoinbaseProductsResult::from_json(json::parse(fx::PRODUCTS_JSON));
    ASSERT_EQ(r.products.size(), 2u);
    EXPECT_EQ(r.products[0].id, "BTC-USD");
    EXPECT_EQ(r.products[1].id, "ETH-USD");
    EXPECT_EQ(r.products[1].base_currency, "ETH");
}

TEST(CoinbaseRestResponses, OrderBookLevel1) {
    auto b = CoinbaseOrderBook::from_json(json::parse(fx::BOOK_L1_JSON));
    EXPECT_EQ(b.sequence, 130929351062);
    ASSERT_EQ(b.bids.size(), 1u);
    ASSERT_EQ(b.asks.size(), 1u);
    EXPECT_DOUBLE_EQ(b.bids[0].price, 63217.42);
    EXPECT_DOUBLE_EQ(b.bids[0].size, 0.00004377);
    EXPECT_EQ(b.bids[0].num_orders, 1);
    EXPECT_DOUBLE_EQ(b.asks[0].price, 63217.43);
    EXPECT_EQ(b.asks[0].num_orders, 8);
}

TEST(CoinbaseRestResponses, OrderBookLevel2) {
    auto b = CoinbaseOrderBook::from_json(json::parse(fx::BOOK_L2_JSON));
    ASSERT_EQ(b.bids.size(), 2u);
    ASSERT_EQ(b.asks.size(), 2u);
    EXPECT_DOUBLE_EQ(b.bids[0].price, 63210.11);
    EXPECT_EQ(b.bids[0].num_orders, 3);
    EXPECT_DOUBLE_EQ(b.asks[1].price, 63210.75);
    EXPECT_EQ(b.asks[1].num_orders, 5);
}

TEST(CoinbaseRestResponses, Ticker) {
    auto t = CoinbaseTicker::from_json(json::parse(fx::TICKER_JSON));
    EXPECT_EQ(t.trade_id, 1040406588);
    EXPECT_DOUBLE_EQ(t.price, 63212.97);
    EXPECT_DOUBLE_EQ(t.bid, 63212.97);
    EXPECT_DOUBLE_EQ(t.ask, 63212.98);
    EXPECT_DOUBLE_EQ(t.size, 0.00782662);
    EXPECT_DOUBLE_EQ(t.volume, 4916.94797396);
    EXPECT_EQ(t.time, "2026-06-19T16:43:33.016274911Z");
}

TEST(CoinbaseRestResponses, Trades) {
    auto r = CoinbaseTradesResult::from_json(json::parse(fx::TRADES_JSON));
    ASSERT_EQ(r.trades.size(), 2u);
    EXPECT_EQ(r.trades[0].trade_id, 1040406590);
    EXPECT_EQ(r.trades[0].side, "buy");
    EXPECT_DOUBLE_EQ(r.trades[0].price, 63212.97);
    EXPECT_DOUBLE_EQ(r.trades[0].size, 0.00003219);
    EXPECT_EQ(r.trades[0].time, "2026-06-19T16:43:33.739704Z");
    EXPECT_EQ(r.trades[1].side, "sell");
}

TEST(CoinbaseRestResponses, Candles) {
    auto r = CoinbaseCandlesResult::from_json(json::parse(fx::CANDLES_JSON));
    ASSERT_EQ(r.candles.size(), 2u);
    EXPECT_EQ(r.candles[0].time, 1781887140);
    EXPECT_DOUBLE_EQ(r.candles[0].low, 63234.99);
    EXPECT_DOUBLE_EQ(r.candles[0].high, 63244.78);
    EXPECT_DOUBLE_EQ(r.candles[0].open, 63244.78);
    EXPECT_DOUBLE_EQ(r.candles[0].close, 63239.75);
    EXPECT_DOUBLE_EQ(r.candles[0].volume, 0.69450254);
    EXPECT_EQ(r.candles[1].time, 1781887080);
}

TEST(CoinbaseRestResponses, Stats) {
    auto s = CoinbaseStats::from_json(json::parse(fx::STATS_JSON));
    EXPECT_DOUBLE_EQ(s.open, 62605.56);
    EXPECT_DOUBLE_EQ(s.high, 63351.3);
    EXPECT_DOUBLE_EQ(s.low, 62159.76);
    EXPECT_DOUBLE_EQ(s.last, 63212.41);
    EXPECT_DOUBLE_EQ(s.volume, 4918.33722403);
    EXPECT_DOUBLE_EQ(s.volume_30day, 269980.64976703);
}
