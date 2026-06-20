// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Verifies Crypto.com public REST response parsing: parse_cryptocom_response
// unwraps the {code,result} envelope and each from_json maps the (terse) wire
// fields. Fixtures are live-captured (cryptocom_rest_example_json.hpp).

#include "exchange/cryptocom/rest_api.hpp"
#include "exchange/cryptocom/types.hpp"

#include "cryptocom_rest_example_json.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace exchange::cryptocom;
using namespace exchange::cryptocom::rest;
using json = nlohmann::json;

namespace {
template<typename Result>
Result parse(const char* fixture) {
    auto resp = parse_cryptocom_response<Result>(200, json::parse(fixture));
    EXPECT_TRUE(resp.ok);
    EXPECT_TRUE(resp.result.has_value());
    return resp.result.value();
}
} // namespace

// ── public/get-instruments ───────────────────────────────────────────────────

TEST(CryptoComRestResponses, Instruments) {
    auto r = parse<CryptoComInstrumentsResult>(cryptocom_fixtures::INSTRUMENTS);
    ASSERT_EQ(r.instruments.size(), 1u);
    const auto& i = r.instruments[0];
    EXPECT_EQ(i.symbol, "BTC_USD");
    EXPECT_EQ(i.inst_type, "CCY_PAIR");
    EXPECT_EQ(i.display_name, "BTC/USD");
    EXPECT_EQ(i.base_ccy, "BTC");
    EXPECT_EQ(i.quote_ccy, "USD");
    EXPECT_EQ(i.quote_decimals, 2);
    EXPECT_EQ(i.quantity_decimals, 5);
    EXPECT_DOUBLE_EQ(i.price_tick_size, 0.01);
    EXPECT_DOUBLE_EQ(i.qty_tick_size, 0.00001);
    EXPECT_TRUE(i.tradable);
}

// ── public/get-tickers ────────────────────────────────────────────────────────

TEST(CryptoComRestResponses, Tickers_TerseKeysMapped) {
    auto r = parse<CryptoComTickersResult>(cryptocom_fixtures::TICKERS);
    ASSERT_EQ(r.tickers.size(), 1u);
    const auto& t = r.tickers[0];
    EXPECT_EQ(t.instrument_name, "BTC_USD");  // i
    EXPECT_DOUBLE_EQ(t.high, 64330.11);       // h
    EXPECT_DOUBLE_EQ(t.low, 62896.85);        // l
    EXPECT_DOUBLE_EQ(t.last, 63778.90);       // a
    EXPECT_DOUBLE_EQ(t.volume, 3025.0123);    // v
    EXPECT_DOUBLE_EQ(t.value, 192262143.88);  // vv
    EXPECT_DOUBLE_EQ(t.change, 0.0119);       // c
    EXPECT_DOUBLE_EQ(t.bid, 63778.69);        // b
    EXPECT_DOUBLE_EQ(t.ask, 63778.70);        // k
    EXPECT_DOUBLE_EQ(t.open_interest, 0.0);   // oi
    EXPECT_EQ(t.timestamp, 1781981527085);    // t
}

// ── public/get-book ───────────────────────────────────────────────────────────

TEST(CryptoComRestResponses, Book_PositionalRows) {
    auto b = parse<CryptoComOrderBook>(cryptocom_fixtures::BOOK);
    EXPECT_EQ(b.instrument_name, "BTC_USD");
    EXPECT_EQ(b.depth, 5);
    EXPECT_EQ(b.t, 1781981527396);
    ASSERT_EQ(b.bids.size(), 2u);
    ASSERT_EQ(b.asks.size(), 2u);
    EXPECT_DOUBLE_EQ(b.bids[0].price, 63778.69);
    EXPECT_DOUBLE_EQ(b.bids[0].size, 0.32644);
    EXPECT_EQ(b.bids[0].num_orders, 6);
    EXPECT_DOUBLE_EQ(b.asks[0].price, 63778.70);
    EXPECT_EQ(b.asks[0].num_orders, 8);
}

// ── public/get-candlestick ────────────────────────────────────────────────────

TEST(CryptoComRestResponses, Candlestick) {
    auto r = parse<CryptoComCandlesResult>(cryptocom_fixtures::CANDLES);
    EXPECT_EQ(r.instrument_name, "BTC_USD");
    EXPECT_EQ(r.interval, "1m");
    ASSERT_EQ(r.candles.size(), 1u);
    const auto& c = r.candles[0];
    EXPECT_EQ(c.t, 1781980080000);
    EXPECT_DOUBLE_EQ(c.o, 63802.28);
    EXPECT_DOUBLE_EQ(c.h, 63810.00);
    EXPECT_DOUBLE_EQ(c.l, 63800.00);
    EXPECT_DOUBLE_EQ(c.c, 63805.10);
    EXPECT_DOUBLE_EQ(c.v, 0.0001);
}

// ── public/get-trades ─────────────────────────────────────────────────────────

TEST(CryptoComRestResponses, Trades) {
    auto r = parse<CryptoComTradesResult>(cryptocom_fixtures::TRADES);
    ASSERT_EQ(r.trades.size(), 1u);
    const auto& t = r.trades[0];
    EXPECT_EQ(t.trade_id, "1781981518402983395");  // d (string id)
    EXPECT_EQ(t.timestamp, 1781981518402);          // t
    EXPECT_DOUBLE_EQ(t.quantity, 0.00109);          // q
    EXPECT_DOUBLE_EQ(t.price, 63778.90);            // p
    EXPECT_EQ(t.side, "buy");                        // s (lowercase wire)
    EXPECT_EQ(t.instrument_name, "BTC_USD");         // i
}
