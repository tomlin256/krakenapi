// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/rest_api.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace exchange::binance::rest;

// ---------------------------------------------------------------------------
// Public requests — verify path, method, and query parameters.
// ---------------------------------------------------------------------------

TEST(BinancePublicRequests, Ping_Path) {
    BinancePingRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ping");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, ServerTime_Path) {
    BinanceServerTimeRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/time");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, TickerPrice_NoParams) {
    BinanceTickerPriceRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ticker/price");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, TickerPrice_SingleSymbol) {
    BinanceTickerPriceRequest req;
    req.symbol = "BTCUSDT";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ticker/price");
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, TickerPrice_MultipleSymbols) {
    BinanceTickerPriceRequest req;
    req.symbols = {"BTCUSDT", "ETHBTC"};
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ticker/price");
    EXPECT_EQ(http.query, "symbols=%5B%22BTCUSDT%22%2C%22ETHBTC%22%5D");
}

TEST(BinancePublicRequests, TickerPrice_SymbolPreferredOverSymbols) {
    BinanceTickerPriceRequest req;
    req.symbol  = "BTCUSDT";
    req.symbols = {"ETHBTC"};
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, OrderBook_SymbolOnly) {
    BinanceOrderBookRequest req;
    req.symbol = "BTCUSDT";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/depth");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, OrderBook_WithLimit) {
    BinanceOrderBookRequest req;
    req.symbol = "BTCUSDT";
    req.limit  = 50;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&limit=50");
}

TEST(BinancePublicRequests, RecentTrades_SymbolOnly) {
    BinanceRecentTradesRequest req;
    req.symbol = "BTCUSDT";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/trades");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, RecentTrades_WithLimit) {
    BinanceRecentTradesRequest req;
    req.symbol = "BTCUSDT";
    req.limit  = 10;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&limit=10");
}

TEST(BinancePublicRequests, Klines_SymbolAndInterval) {
    BinanceKlinesRequest req;
    req.symbol   = "BTCUSDT";
    req.interval = "1m";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/klines");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m");
}

TEST(BinancePublicRequests, Klines_AllOptionals_InOrder) {
    BinanceKlinesRequest req;
    req.symbol     = "BTCUSDT";
    req.interval   = "1h";
    req.start_time = 1499040000000LL;
    req.end_time   = 1499644799999LL;
    req.limit      = 500;
    auto http = req.build();
    EXPECT_EQ(http.query,
              "symbol=BTCUSDT&interval=1h"
              "&startTime=1499040000000&endTime=1499644799999&limit=500");
}

TEST(BinancePublicRequests, Klines_OnlyStartTime) {
    BinanceKlinesRequest req;
    req.symbol     = "BTCUSDT";
    req.interval   = "1m";
    req.start_time = 1499040000000LL;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m&startTime=1499040000000");
}

TEST(BinancePublicRequests, Klines_OnlyEndTime) {
    BinanceKlinesRequest req;
    req.symbol   = "BTCUSDT";
    req.interval = "1m";
    req.end_time = 1499644799999LL;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m&endTime=1499644799999");
}

TEST(BinancePublicRequests, Klines_OnlyLimit) {
    BinanceKlinesRequest req;
    req.symbol   = "BTCUSDT";
    req.interval = "1m";
    req.limit    = 100;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m&limit=100");
}
