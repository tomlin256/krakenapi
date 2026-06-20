// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Verifies that each Crypto.com REST request builds the correct HTTP method,
// path, and query string. Response parsing is covered in
// test_cryptocom_rest_responses.cpp.

#include "exchange/cryptocom/rest_api.hpp"

#include <gtest/gtest.h>

using namespace exchange::cryptocom::rest;
using M = HttpRequest::Method;

// ── Public endpoints ──────────────────────────────────────────────────────────

TEST(CryptoComRestRequests, Instruments) {
    auto r = CryptoComInstrumentsRequest{}.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/exchange/v1/public/get-instruments");
    EXPECT_TRUE(r.query.empty());
    EXPECT_TRUE(r.body.empty());
}

TEST(CryptoComRestRequests, Tickers_AllInstruments_NoQuery) {
    auto r = CryptoComTickersRequest{}.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/exchange/v1/public/get-tickers");
    EXPECT_TRUE(r.query.empty());
}

TEST(CryptoComRestRequests, Tickers_SingleInstrument) {
    CryptoComTickersRequest req;
    req.instrument_name = "BTC_USD";
    auto r = req.build();
    EXPECT_EQ(r.path, "/exchange/v1/public/get-tickers");
    EXPECT_EQ(r.query, "instrument_name=BTC_USD");
}

TEST(CryptoComRestRequests, Book_WithDepth) {
    CryptoComOrderBookRequest req;
    req.instrument_name = "BTC_USD";
    req.depth           = 10;
    auto r = req.build();
    EXPECT_EQ(r.path, "/exchange/v1/public/get-book");
    EXPECT_EQ(r.query, "instrument_name=BTC_USD&depth=10");
}

TEST(CryptoComRestRequests, Book_WithoutDepth) {
    CryptoComOrderBookRequest req;
    req.instrument_name = "ETH_USD";
    auto r = req.build();
    EXPECT_EQ(r.query, "instrument_name=ETH_USD");
}

TEST(CryptoComRestRequests, Candlestick_AllParams) {
    CryptoComCandlesRequest req;
    req.instrument_name = "BTC_USD";
    req.timeframe       = "5m";
    req.count           = 25;
    auto r = req.build();
    EXPECT_EQ(r.path, "/exchange/v1/public/get-candlestick");
    EXPECT_EQ(r.query, "instrument_name=BTC_USD&timeframe=5m&count=25");
}

TEST(CryptoComRestRequests, Candlestick_TimeframeOnly) {
    CryptoComCandlesRequest req;
    req.instrument_name = "BTC_USD";
    req.timeframe       = "1m";
    auto r = req.build();
    EXPECT_EQ(r.query, "instrument_name=BTC_USD&timeframe=1m");
}

TEST(CryptoComRestRequests, Trades_WithCount) {
    CryptoComTradesRequest req;
    req.instrument_name = "BTC_USD";
    req.count           = 50;
    auto r = req.build();
    EXPECT_EQ(r.path, "/exchange/v1/public/get-trades");
    EXPECT_EQ(r.query, "instrument_name=BTC_USD&count=50");
}

TEST(CryptoComRestRequests, Trades_InstrumentOnly) {
    CryptoComTradesRequest req;
    req.instrument_name = "BTC_USD";
    auto r = req.build();
    EXPECT_EQ(r.query, "instrument_name=BTC_USD");
}
