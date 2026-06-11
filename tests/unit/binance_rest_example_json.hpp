// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Sample JSON messages captured from the Binance Spot REST API docs (see
// docs/plans/001-appendix-binance-message-formats.md, fetched 2026-06-06).
// Used as test fixtures in test_binance_rest_responses.cpp.
//
// Fixtures are verbatim from the appendix unless marked synthetic.

#pragma once

namespace exchange::binance::rest::test {

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v3/ping
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kPingJson = R"({})";

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v3/time
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kServerTimeJson = R"({"serverTime": 1499827319559})";

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v3/ticker/price
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kTickerPriceSingleJson = R"({"symbol":"LTCBTC","price":"4.00000200"})";

// Synthetic — array form returned for a `symbols=[...]` query (2 elements).
inline constexpr const char* kTickerPriceArrayJson =
    R"([{"symbol":"LTCBTC","price":"4.00000200"},{"symbol":"ETHBTC","price":"0.07734100"}])";

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v3/depth
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kDepthJson = R"({
  "lastUpdateId": 1027024,
  "bids": [["4.00000000","431.00000000"]],
  "asks": [["4.00000200","12.00000000"]]
})";

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v3/trades
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kTradesJson = R"([
  {"id":28457,"price":"4.00000100","qty":"12.00000000","quoteQty":"48.000012","time":1499865549590,"isBuyerMaker":true,"isBestMatch":true}
])";

// ─────────────────────────────────────────────────────────────────────────────
// Error envelope (any endpoint, non-2xx status)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kErrorJson = R"({"code":-1121,"msg":"Invalid symbol."})";

} // namespace exchange::binance::rest::test
