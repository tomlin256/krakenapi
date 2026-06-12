// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Binance WebSocket market-stream unit tests — frame descriptor classification,
// ack parsing, event from_json field assertions, and the full subscribe
// lifecycle through ExchangeWsClient with a mock connection. No network I/O.

#include "exchange/binance/ws_streams.hpp"
#include "binance_ws_stream_example_json.hpp"

#include <gtest/gtest.h>

using namespace exchange::binance::ws;
namespace fixtures = exchange::binance::ws::test;

// ─────────────────────────────────────────────────────────────────────────────
// binance_stream_frame_descriptor — appendix §5 classification
// ─────────────────────────────────────────────────────────────────────────────

TEST(BinanceStreamDescriptor, SuccessAck_IsMethodResponse) {
    auto d = binance_stream_frame_descriptor(json::parse(fixtures::kSubscribeAckJson));
    EXPECT_EQ(d.kind, FrameKind::MethodResponse);
    ASSERT_TRUE(d.correlation_id.has_value());
    EXPECT_EQ(*d.correlation_id, "1");
    EXPECT_TRUE(d.route_key.empty());
}

TEST(BinanceStreamDescriptor, ErrorAck_IsMethodResponse) {
    auto d = binance_stream_frame_descriptor(json::parse(fixtures::kErrorAckJson));
    EXPECT_EQ(d.kind, FrameKind::MethodResponse);
    ASSERT_TRUE(d.correlation_id.has_value());
    EXPECT_EQ(*d.correlation_id, "7");
}

TEST(BinanceStreamDescriptor, WrappedPush_RoutesByStreamName) {
    auto d = binance_stream_frame_descriptor(json::parse(fixtures::kWrappedAggTradeJson));
    EXPECT_EQ(d.kind, FrameKind::PushMessage);
    EXPECT_EQ(d.route_key, "bnbbtc@aggTrade");
    EXPECT_FALSE(d.correlation_id.has_value());
}

TEST(BinanceStreamDescriptor, EmptyObject_IsUnknown) {
    auto d = binance_stream_frame_descriptor(json::parse("{}"));
    EXPECT_EQ(d.kind, FrameKind::Unknown);
}

TEST(BinanceStreamDescriptor, NullIdErrorFrame_IsUnknown) {
    // Binance sends id:null when the request itself was malformed JSON —
    // uncorrelatable, so the descriptor must not invent a correlation id.
    auto d = binance_stream_frame_descriptor(
        json::parse(R"({"error":{"code":2,"msg":"Invalid request"},"id":null})"));
    EXPECT_EQ(d.kind, FrameKind::Unknown);
    EXPECT_FALSE(d.correlation_id.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// BinanceStreamAck::from_json
// ─────────────────────────────────────────────────────────────────────────────

TEST(BinanceStreamAck, SuccessAck_FromJson) {
    auto a = BinanceStreamAck::from_json(json::parse(fixtures::kSubscribeAckJson));
    EXPECT_TRUE(a.success);
    EXPECT_FALSE(a.error.has_value());
    EXPECT_EQ(a.id, 1);
}

TEST(BinanceStreamAck, ErrorAck_FromJson) {
    auto a = BinanceStreamAck::from_json(json::parse(fixtures::kErrorAckJson));
    EXPECT_FALSE(a.success);
    ASSERT_TRUE(a.error.has_value());
    EXPECT_EQ(*a.error, "Invalid request: subscription id not provided");
    EXPECT_EQ(a.id, 7);
}

// ─────────────────────────────────────────────────────────────────────────────
// Flat push event types — from_json field assertions (Step 7.2)
// ─────────────────────────────────────────────────────────────────────────────

TEST(BinanceStreamEvents, AggTrade_FromJson) {
    auto e = BinanceAggTradeEvent::from_json(json::parse(fixtures::kAggTradeJson));
    EXPECT_EQ(e.event_time, 1672515782136LL);
    EXPECT_EQ(e.symbol, "BNBBTC");
    EXPECT_EQ(e.agg_trade_id, 12345);
    EXPECT_DOUBLE_EQ(e.price, 0.001);
    EXPECT_DOUBLE_EQ(e.qty, 100.0);
    EXPECT_EQ(e.first_trade_id, 100);
    EXPECT_EQ(e.last_trade_id, 105);
    EXPECT_EQ(e.trade_time, 1672515782136LL);
    EXPECT_TRUE(e.is_buyer_maker);
}

// Pins the decision-4 unwrap rule: dispatch hands push callbacks the whole
// {"stream","data"} frame, and from_json must parse it identically to the
// bare payload.
TEST(BinanceStreamEvents, AggTrade_ParsesWrappedFrame) {
    auto bare    = BinanceAggTradeEvent::from_json(json::parse(fixtures::kAggTradeJson));
    auto wrapped = BinanceAggTradeEvent::from_json(json::parse(fixtures::kWrappedAggTradeJson));
    EXPECT_EQ(wrapped.event_time, bare.event_time);
    EXPECT_EQ(wrapped.symbol, bare.symbol);
    EXPECT_EQ(wrapped.agg_trade_id, bare.agg_trade_id);
    EXPECT_DOUBLE_EQ(wrapped.price, bare.price);
    EXPECT_DOUBLE_EQ(wrapped.qty, bare.qty);
    EXPECT_EQ(wrapped.first_trade_id, bare.first_trade_id);
    EXPECT_EQ(wrapped.last_trade_id, bare.last_trade_id);
    EXPECT_EQ(wrapped.trade_time, bare.trade_time);
    EXPECT_EQ(wrapped.is_buyer_maker, bare.is_buyer_maker);
}

TEST(BinanceStreamEvents, Trade_FromJson) {
    auto e = BinanceTradeEvent::from_json(json::parse(fixtures::kTradeJson));
    EXPECT_EQ(e.event_time, 1672515782136LL);
    EXPECT_EQ(e.symbol, "BNBBTC");
    EXPECT_EQ(e.trade_id, 12345);
    EXPECT_DOUBLE_EQ(e.price, 0.001);
    EXPECT_DOUBLE_EQ(e.qty, 100.0);
    EXPECT_EQ(e.trade_time, 1672515782136LL);
    EXPECT_TRUE(e.is_buyer_maker);
}

TEST(BinanceStreamEvents, Ticker_FromJson) {
    auto e = BinanceTickerEvent::from_json(json::parse(fixtures::kTickerJson));
    EXPECT_EQ(e.event_time, 1672515782136LL);
    EXPECT_EQ(e.symbol, "BNBBTC");
    EXPECT_DOUBLE_EQ(e.price_change, 0.0015);
    EXPECT_DOUBLE_EQ(e.price_change_pct, 250.00);
    EXPECT_DOUBLE_EQ(e.weighted_avg_price, 0.0018);
    EXPECT_DOUBLE_EQ(e.prev_close, 0.0009);
    EXPECT_DOUBLE_EQ(e.last_price, 0.0025);
    EXPECT_DOUBLE_EQ(e.last_qty, 10.0);
    EXPECT_DOUBLE_EQ(e.bid_price, 0.0024);
    EXPECT_DOUBLE_EQ(e.bid_qty, 10.0);
    EXPECT_DOUBLE_EQ(e.ask_price, 0.0026);
    EXPECT_DOUBLE_EQ(e.ask_qty, 100.0);
    EXPECT_DOUBLE_EQ(e.open, 0.0010);
    EXPECT_DOUBLE_EQ(e.high, 0.0025);
    EXPECT_DOUBLE_EQ(e.low, 0.0010);
    EXPECT_DOUBLE_EQ(e.volume, 10000.0);
    EXPECT_DOUBLE_EQ(e.quote_volume, 18.0);
    EXPECT_EQ(e.stats_open_time, 0);
    EXPECT_EQ(e.stats_close_time, 86400000);
    EXPECT_EQ(e.first_trade_id, 0);
    EXPECT_EQ(e.last_trade_id, 18150);
    EXPECT_EQ(e.num_trades, 18151);
}

TEST(BinanceStreamEvents, MiniTicker_FromJson) {
    auto e = BinanceMiniTickerEvent::from_json(json::parse(fixtures::kMiniTickerJson));
    EXPECT_EQ(e.event_time, 1672515782136LL);
    EXPECT_EQ(e.symbol, "BNBBTC");
    EXPECT_DOUBLE_EQ(e.close, 0.0025);
    EXPECT_DOUBLE_EQ(e.open, 0.0010);
    EXPECT_DOUBLE_EQ(e.high, 0.0025);
    EXPECT_DOUBLE_EQ(e.low, 0.0010);
    EXPECT_DOUBLE_EQ(e.volume, 10000.0);
    EXPECT_DOUBLE_EQ(e.quote_volume, 18.0);
}

TEST(BinanceStreamEvents, BookTicker_FromJson) {
    auto e = BinanceBookTickerEvent::from_json(json::parse(fixtures::kBookTickerJson));
    EXPECT_EQ(e.update_id, 400900217);
    EXPECT_EQ(e.symbol, "BNBUSDT");
    EXPECT_DOUBLE_EQ(e.bid_price, 25.3519);
    EXPECT_DOUBLE_EQ(e.bid_qty, 31.21);
    EXPECT_DOUBLE_EQ(e.ask_price, 25.3652);
    EXPECT_DOUBLE_EQ(e.ask_qty, 40.66);
}
