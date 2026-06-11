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
