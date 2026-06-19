// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Coinbase Exchange WebSocket feed fixtures for test_coinbase_ws_client.cpp.
// These follow the documented, long-stable Coinbase feed message formats
// (docs.cdp.coinbase.com/exchange/websocket-feed). The market-data frames match
// the documented public shapes; the user-channel lifecycle frames are synthetic
// (the user channel is authenticated and tested with synthetic fixtures, per the
// plan). All are network-free string literals.

#pragma once

namespace coinbase_ws_fixtures {

// Subscribe confirmation (full-state echo — no correlation id).
inline constexpr const char* SUBSCRIPTIONS_JSON =
    R"({"type":"subscriptions","channels":[{"name":"ticker","product_ids":["BTC-USD"]}]})";

// ticker channel
inline constexpr const char* TICKER_JSON =
    R"({"type":"ticker","sequence":7100000001,"product_id":"BTC-USD","price":"63212.97",)"
    R"("open_24h":"62605.56","volume_24h":"4916.94797396","low_24h":"62159.76","high_24h":"63351.30",)"
    R"("volume_30d":"269980.64976703","best_bid":"63212.97","best_bid_size":"0.10",)"
    R"("best_ask":"63212.98","best_ask_size":"0.05","side":"buy","time":"2026-06-19T16:43:33.016Z",)"
    R"("trade_id":1040406588,"last_size":"0.00782662"})";

// level2 snapshot
inline constexpr const char* SNAPSHOT_JSON =
    R"({"type":"snapshot","product_id":"BTC-USD",)"
    R"("bids":[["63210.11","1.5"],["63210.10","0.2"]],)"
    R"("asks":[["63210.50","0.8"],["63210.75","2.1"]]})";

// level2 update
inline constexpr const char* L2UPDATE_JSON =
    R"({"type":"l2update","product_id":"BTC-USD","time":"2026-06-19T16:43:34.000Z",)"
    R"("changes":[["buy","63210.11","0.30000000"],["sell","63210.50","0.00000000"]]})";

// matches channel
inline constexpr const char* MATCH_JSON =
    R"({"type":"match","trade_id":1040406589,"sequence":7100000002,)"
    R"("maker_order_id":"m1b2c3d4-0000-4000-8000-000000000001",)"
    R"("taker_order_id":"t9b8c7d6-0000-4000-8000-000000000002",)"
    R"("time":"2026-06-19T16:43:33.366Z","product_id":"BTC-USD","size":"0.00195786",)"
    R"("price":"63212.98","side":"sell"})";

// heartbeat channel
inline constexpr const char* HEARTBEAT_JSON =
    R"({"type":"heartbeat","last_trade_id":1040406590,"product_id":"BTC-USD",)"
    R"("sequence":7100000003,"time":"2026-06-19T16:43:34.500Z"})";

// error frame (e.g. bad channel/product or auth failure)
inline constexpr const char* ERROR_JSON =
    R"({"type":"error","message":"Failed to subscribe","reason":"ticker is not a valid product"})";

// user/full channel — "received" lifecycle frame (synthetic)
inline constexpr const char* USER_RECEIVED_JSON =
    R"({"type":"received","time":"2026-06-19T10:00:00.000Z","product_id":"BTC-USD",)"
    R"("sequence":7100000010,"order_id":"d0c5a4f3-0000-4000-8000-000000000010",)"
    R"("order_type":"limit","size":"0.01000000","price":"30000.00","side":"buy",)"
    R"("client_oid":"abc-123"})";

// user/full channel — "done" (filled) lifecycle frame (synthetic)
inline constexpr const char* USER_DONE_JSON =
    R"({"type":"done","time":"2026-06-19T10:05:00.000Z","product_id":"BTC-USD",)"
    R"("sequence":7100000011,"price":"30000.00","order_id":"d0c5a4f3-0000-4000-8000-000000000010",)"
    R"("reason":"filled","side":"buy","remaining_size":"0.00000000"})";

} // namespace coinbase_ws_fixtures
