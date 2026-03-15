// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Sample JSON messages captured from the Kraken WebSocket v2 API (2026-03-15).
// Used as test fixtures in test_ws_responses.cpp.
//
// Push channel messages (ticker, book, trade, ohlc, status, heartbeat) are
// real frames from the live API.  Method responses (pong, add_order, …) are
// synthetic because they require private credentials or a prior request.
// The instrument channel snapshot is also synthetic — see the comment there.

#pragma once

namespace kraken::ws::test {

// ─────────────────────────────────────────────────────────────────────────────
// Admin — Status / Heartbeat
// ─────────────────────────────────────────────────────────────────────────────

// Real frame received on every new connection.
inline constexpr const char* kStatusUpdateJson = R"({"channel":"status","data":[{"api_version":"v2","connection_id":8869243459854524911,"system":"online","version":"2.0.10"}],"type":"update"})";

// Real frame sent by the server every second.
inline constexpr const char* kHeartbeatJson = R"({"channel":"heartbeat"})";

// ─────────────────────────────────────────────────────────────────────────────
// Subscribe response
// ─────────────────────────────────────────────────────────────────────────────

// Real subscribe-ack for the ticker channel (no req_id — not included in
// the outbound subscribe request, so the server does not echo one back).
inline constexpr const char* kSubscribeResponseJson = R"({"method":"subscribe","result":{"channel":"ticker","event_trigger":"trades","snapshot":true,"symbol":"BTC/USD"},"success":true,"time_in":"2026-03-15T11:25:20.538603Z","time_out":"2026-03-15T11:25:20.538627Z"})";

// ─────────────────────────────────────────────────────────────────────────────
// Ticker (Level 1)
// ─────────────────────────────────────────────────────────────────────────────

// Real snapshot frame — sent immediately after a successful subscribe.
inline constexpr const char* kTickerSnapshotJson = R"({"channel":"ticker","data":[{"ask":71770.4,"ask_qty":1.72978253,"bid":71770.3,"bid_qty":0.03155564,"change":1087.6,"change_pct":1.54,"high":72000.0,"last":71770.4,"low":70510.9,"symbol":"BTC/USD","timestamp":"2026-03-15T11:25:19.200068Z","volume":636.8977771,"vwap":71318.4}],"type":"snapshot"})";

// ─────────────────────────────────────────────────────────────────────────────
// Book (Level 2)
// ─────────────────────────────────────────────────────────────────────────────

// Real depth-10 snapshot for BTC/USD.
inline constexpr const char* kBookSnapshotJson = R"({"channel":"book","data":[{"asks":[{"price":71770.4,"qty":1.80783344},{"price":71770.6,"qty":5.1e-05},{"price":71770.8,"qty":0.69666257},{"price":71771.7,"qty":0.69665386},{"price":71772.0,"qty":0.01617936},{"price":71774.2,"qty":5.1e-05},{"price":71774.6,"qty":0.69662614},{"price":71774.7,"qty":0.06},{"price":71777.5,"qty":0.009},{"price":71777.6,"qty":0.0776}],"bids":[{"price":71770.3,"qty":0.03228444},{"price":71769.0,"qty":0.002348},{"price":71767.1,"qty":5.1e-05},{"price":71763.5,"qty":5.1e-05},{"price":71760.5,"qty":0.11146517},{"price":71760.4,"qty":0.15525921},{"price":71760.0,"qty":5.1e-05},{"price":71758.6,"qty":0.02},{"price":71758.1,"qty":1.210778},{"price":71756.4,"qty":5.1e-05}],"checksum":2552662837,"symbol":"BTC/USD","timestamp":"2026-03-15T11:25:44.470825Z"}],"type":"snapshot"})";

// ─────────────────────────────────────────────────────────────────────────────
// Trade
// ─────────────────────────────────────────────────────────────────────────────

// Real trade update — note trade_id is an integer as returned by the API.
inline constexpr const char* kTradeUpdateJson = R"({"channel":"trade","data":[{"ord_type":"limit","price":71749.0,"qty":0.00220616,"side":"sell","symbol":"BTC/USD","timestamp":"2026-03-15T11:26:23.023215Z","trade_id":97184416}],"type":"update"})";

// ─────────────────────────────────────────────────────────────────────────────
// OHLC / Candles
// ─────────────────────────────────────────────────────────────────────────────

// Real 1-minute candle update.  interval_begin is an ISO 8601 timestamp
// (not an integer); interval carries the candle width in minutes.
inline constexpr const char* kOhlcUpdateJson = R"({"channel":"ohlc","data":[{"close":71712.0,"high":71749.1,"interval":1,"interval_begin":"2026-03-15T11:26:00.000000000Z","low":71712.0,"open":71749.1,"symbol":"BTC/USD","timestamp":"2026-03-15T11:27:00.000000Z","trades":14,"volume":0.06060006,"vwap":71738.2}],"timestamp":"2026-03-15T11:26:42.303671050Z","type":"update"})";

// ─────────────────────────────────────────────────────────────────────────────
// Instrument
// ─────────────────────────────────────────────────────────────────────────────

// Synthetic snapshot matching the real instrument channel structure:
// data is an object with "assets" and "pairs" arrays.
inline constexpr const char* kInstrumentSnapshotJson = R"({"channel":"instrument","data":{"assets":[{"borrowable":true,"collateral_value":1.0,"id":"BTC","margin_rate":0.02,"precision":10,"precision_display":5,"status":"enabled"}],"pairs":[{"base":"BTC","cost_min":0.5,"cost_precision":5,"has_index":true,"margin_initial":20,"position_limit_long":250,"position_limit_short":250,"price_increment":0.1,"qty_increment":0.00000001,"qty_min":0.0001,"qty_precision":10,"quote":"USD","status":"online","symbol":"BTC/USD"}]},"type":"snapshot"})";

// ─────────────────────────────────────────────────────────────────────────────
// Executions / Balances (private channels — synthetic examples)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kExecutionsSnapshotJson = R"({"channel":"executions","data":[{"avg_price":50000.0,"cl_ord_id":"my-order-1","cost":5000.0,"cum_qty":0.1,"exec_id":"EXEC-001","exec_type":"filled","fee":2.5,"fee_currency":"USD","last_price":50000.0,"last_qty":0.1,"leaves_qty":0.0,"limit_price":50000.0,"margin":false,"order_id":"ORDER-001","order_qty":0.1,"order_status":"filled","order_type":"limit","post_only":false,"side":"buy","symbol":"BTC/USD","time_in_force":"GTC","timestamp":"2026-03-15T12:00:00.000Z"}],"type":"snapshot"})";

inline constexpr const char* kBalancesSnapshotJson = R"({"channel":"balances","data":[{"asset":"BTC","balance":1.5,"hold_trade":0.1},{"asset":"USD","balance":25000.0,"hold_trade":5000.0}],"type":"snapshot"})";

// ─────────────────────────────────────────────────────────────────────────────
// Method responses (synthetic — require private credentials or a prior request)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kPongJson = R"({"method":"pong","req_id":42})";

inline constexpr const char* kAddOrderResponseJson = R"({"method":"add_order","req_id":2,"result":{"cl_ord_id":"my-order-1","order_id":"ORDER-001","order_userref":12345},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

inline constexpr const char* kAmendOrderResponseJson = R"({"method":"amend_order","req_id":3,"result":{"cl_ord_id":"my-order-1","order_id":"ORDER-001"},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

inline constexpr const char* kCancelOrderResponseJson = R"({"method":"cancel_order","req_id":4,"result":{"orders_cancelled":[{"order_id":"ORDER-001","success":true}]},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

inline constexpr const char* kCancelAllResponseJson = R"({"method":"cancel_all","req_id":5,"result":{"count":3},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

inline constexpr const char* kCancelOnDisconnectResponseJson = R"({"method":"cancel_after","req_id":6,"result":{"current_time":"2026-03-15T12:00:00.000Z","trigger_time":"2026-03-15T12:01:00.000Z"},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

inline constexpr const char* kBatchAddResponseJson = R"({"method":"batch_add","req_id":7,"result":{"orders":[{"cl_ord_id":"batch-1","order_id":"ORDER-A","success":true},{"order_id":"ORDER-B","success":true}]},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

inline constexpr const char* kBatchCancelResponseJson = R"({"method":"batch_cancel","req_id":8,"result":{"orders_cancelled":2},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

inline constexpr const char* kEditOrderResponseJson = R"({"method":"edit_order","req_id":9,"result":{"cl_ord_id":"edit-1","order_id":"ORDER-002","original_order_id":"ORDER-001"},"success":true,"time_in":"2026-03-15T12:00:00.000Z","time_out":"2026-03-15T12:00:00.001Z"})";

} // namespace kraken::ws::test
