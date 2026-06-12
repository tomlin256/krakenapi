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
