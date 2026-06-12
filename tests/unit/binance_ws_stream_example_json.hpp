// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Sample JSON frames from the Binance WebSocket market-stream docs (see
// docs/plans/001-appendix-binance-message-formats.md §3, fetched 2026-06-06).
// Used as test fixtures in test_binance_ws_client.cpp.
//
// Fixtures are verbatim from the appendix unless marked synthetic.

#pragma once

namespace exchange::binance::ws::test {

// ─────────────────────────────────────────────────────────────────────────────
// SUBSCRIBE / UNSUBSCRIBE acks
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kSubscribeAckJson = R"({"result":null,"id":1})";

inline constexpr const char* kErrorAckJson =
    R"({"error":{"code":2,"msg":"Invalid request: subscription id not provided"},"id":7})";

// ─────────────────────────────────────────────────────────────────────────────
// Combined-stream wrapper — what dispatch actually delivers to push callbacks
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kWrappedAggTradeJson = R"({
  "stream": "bnbbtc@aggTrade",
  "data": {"e":"aggTrade","E":1672515782136,"s":"BNBBTC","a":12345,"p":"0.001","q":"100","f":100,"l":105,"T":1672515782136,"m":true,"M":true}
})";

} // namespace exchange::binance::ws::test
