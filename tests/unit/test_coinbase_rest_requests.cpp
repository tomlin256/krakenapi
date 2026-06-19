// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Verifies that each Coinbase REST request builds the correct HTTP method,
// path, and query string. Response parsing is covered separately in
// test_coinbase_rest_responses.cpp.

#include "exchange/coinbase/rest_api.hpp"

#include <gtest/gtest.h>

using namespace exchange::coinbase::rest;
using M = HttpRequest::Method;

// ── Public endpoints ──────────────────────────────────────────────────────────

TEST(CoinbaseRestRequests, ServerTime) {
    auto r = CoinbaseServerTimeRequest{}.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/time");
    EXPECT_TRUE(r.query.empty());
    EXPECT_TRUE(r.body.empty());
}

TEST(CoinbaseRestRequests, Products) {
    auto r = CoinbaseProductsRequest{}.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/products");
    EXPECT_TRUE(r.query.empty());
}

TEST(CoinbaseRestRequests, SingleProduct) {
    CoinbaseProductRequest req;
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/products/BTC-USD");
}

TEST(CoinbaseRestRequests, OrderBook_WithLevel) {
    CoinbaseOrderBookRequest req;
    req.product_id = "BTC-USD";
    req.level      = 2;
    auto r = req.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/products/BTC-USD/book");
    EXPECT_EQ(r.query, "level=2");
}

TEST(CoinbaseRestRequests, OrderBook_WithoutLevel_NoQuery) {
    CoinbaseOrderBookRequest req;
    req.product_id = "ETH-EUR";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/ETH-EUR/book");
    EXPECT_TRUE(r.query.empty());
}

TEST(CoinbaseRestRequests, Ticker) {
    CoinbaseTickerRequest req;
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/ticker");
    EXPECT_TRUE(r.query.empty());
}

TEST(CoinbaseRestRequests, Trades_WithLimit) {
    CoinbaseTradesRequest req;
    req.product_id = "BTC-USD";
    req.limit      = 50;
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/trades");
    EXPECT_EQ(r.query, "limit=50");
}

TEST(CoinbaseRestRequests, Candles_AllParams) {
    CoinbaseCandlesRequest req;
    req.product_id  = "BTC-USD";
    req.granularity = 60;
    req.start       = "2026-06-19T00:00:00Z";
    req.end         = "2026-06-19T01:00:00Z";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/candles");
    EXPECT_EQ(r.query,
              "granularity=60&start=2026-06-19T00:00:00Z&end=2026-06-19T01:00:00Z");
}

TEST(CoinbaseRestRequests, Candles_GranularityOnly) {
    CoinbaseCandlesRequest req;
    req.product_id  = "BTC-USD";
    req.granularity = 3600;
    auto r = req.build();
    EXPECT_EQ(r.query, "granularity=3600");
}

TEST(CoinbaseRestRequests, Stats) {
    CoinbaseStatsRequest req;
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/stats");
}
