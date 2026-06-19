// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// SYNTHETIC Coinbase Exchange *private* REST fixtures used by
// test_coinbase_rest_responses.cpp and test_coinbase_client.cpp. Unlike the
// public fixtures, these are not live captures (the plan verifies private paths
// with synthetic fixtures only — no credentials). Field names and shapes follow
// the documented Coinbase Exchange responses (docs.cdp.coinbase.com/exchange).

#pragma once

namespace coinbase_fixtures {

// GET /accounts  — array (synthetic)
inline constexpr const char* ACCOUNTS_JSON =
    R"([{"id":"7fd0abc1-0000-4000-8000-000000000001","currency":"USD",)"
    R"("balance":"1000.0000000000000000","hold":"50.0000000000000000",)"
    R"("available":"950.0000000000000000","profile_id":"8058d771-0000-4000-8000-000000000aaa",)"
    R"("trading_enabled":true},)"
    R"({"id":"7fd0abc1-0000-4000-8000-000000000002","currency":"BTC",)"
    R"("balance":"0.50000000","hold":"0.00000000","available":"0.50000000",)"
    R"("profile_id":"8058d771-0000-4000-8000-000000000aaa","trading_enabled":true}])";

// GET /accounts/{id}  — single (synthetic)
inline constexpr const char* ACCOUNT_JSON =
    R"({"id":"7fd0abc1-0000-4000-8000-000000000001","currency":"USD",)"
    R"("balance":"1000.0000000000000000","hold":"50.0000000000000000",)"
    R"("available":"950.0000000000000000","profile_id":"8058d771-0000-4000-8000-000000000aaa",)"
    R"("trading_enabled":true})";

// POST /orders response — open limit order (synthetic)
inline constexpr const char* ORDER_OPEN_JSON =
    R"({"id":"d0c5a4f3-0000-4000-8000-000000000010","price":"30000.00000000",)"
    R"("size":"0.01000000","product_id":"BTC-USD","side":"buy","type":"limit",)"
    R"("time_in_force":"GTC","post_only":true,"created_at":"2026-06-19T10:00:00.000000Z",)"
    R"("fill_fees":"0.0000000000000000","filled_size":"0.00000000",)"
    R"("executed_value":"0.0000000000000000","status":"open","settled":false})";

// GET /orders/{id} — a filled (done) order (synthetic)
inline constexpr const char* ORDER_DONE_JSON =
    R"({"id":"a1b2c3d4-0000-4000-8000-000000000011","price":"30000.00000000",)"
    R"("size":"0.01000000","product_id":"BTC-USD","side":"sell","type":"limit",)"
    R"("time_in_force":"GTC","post_only":false,"created_at":"2026-06-19T10:00:00.000000Z",)"
    R"("done_at":"2026-06-19T10:05:00.000000Z","done_reason":"filled",)"
    R"("fill_fees":"1.5000000000000000","filled_size":"0.01000000",)"
    R"("executed_value":"300.0000000000000000","status":"done","settled":true})";

// GET /orders — array (synthetic)
inline constexpr const char* ORDERS_JSON =
    R"([{"id":"d0c5a4f3-0000-4000-8000-000000000010","price":"30000.00000000",)"
    R"("size":"0.01000000","product_id":"BTC-USD","side":"buy","type":"limit",)"
    R"("time_in_force":"GTC","post_only":true,"created_at":"2026-06-19T10:00:00.000000Z",)"
    R"("fill_fees":"0.0","filled_size":"0.0","executed_value":"0.0","status":"open","settled":false},)"
    R"({"id":"e1f2a3b4-0000-4000-8000-000000000012","size":"0.00500000",)"
    R"("funds":"150.00","product_id":"BTC-USD","side":"buy","type":"market",)"
    R"("post_only":false,"created_at":"2026-06-19T10:02:00.000000Z",)"
    R"("fill_fees":"0.0","filled_size":"0.0","executed_value":"0.0","status":"pending","settled":false}])";

// DELETE /orders/{id} — bare JSON string of the cancelled id (synthetic)
inline constexpr const char* CANCEL_ONE_JSON =
    R"("d0c5a4f3-0000-4000-8000-000000000010")";

// DELETE /orders — array of cancelled id strings (synthetic)
inline constexpr const char* CANCEL_ALL_JSON =
    R"(["d0c5a4f3-0000-4000-8000-000000000010","e1f2a3b4-0000-4000-8000-000000000012"])";

// GET /fills — array (synthetic)
inline constexpr const char* FILLS_JSON =
    R"([{"trade_id":74,"product_id":"BTC-USD","order_id":"a1b2c3d4-0000-4000-8000-000000000011",)"
    R"("liquidity":"T","price":"30000.00000000","size":"0.01000000","fee":"1.5000000000000000",)"
    R"("created_at":"2026-06-19T10:05:00.000000Z","side":"sell","settled":true}])";

} // namespace coinbase_fixtures
