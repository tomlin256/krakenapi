// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Sample JSON messages for the Binance Spot signed (private) REST endpoints
// (see docs/plans/001-appendix-binance-message-formats.md §2, fetched
// 2026-06-06). Used as test fixtures in test_binance_rest_responses.cpp and
// test_binance_client.cpp.
//
// All fixtures here are synthetic: these endpoints are credential-gated, so
// frames cannot be captured live — shapes are verbatim from the documented
// examples in the appendix.

#pragma once

namespace exchange::binance::rest::test {

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v3/account
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kAccountJson = R"({
  "makerCommission":15,"takerCommission":15,"buyerCommission":0,"sellerCommission":0,
  "commissionRates":{"maker":"0.00150000","taker":"0.00150000","buyer":"0.00000000","seller":"0.00000000"},
  "canTrade":true,"canWithdraw":true,"canDeposit":true,"brokered":false,
  "requireSelfTradePrevention":false,"preventSor":false,"updateTime":123456789,
  "accountType":"SPOT",
  "balances":[
    {"asset":"BTC","free":"4723846.89208129","locked":"0.00000000"},
    {"asset":"LTC","free":"4763368.68006011","locked":"0.00000000"}
  ],
  "permissions":["SPOT"],"uid":354937868
})";

} // namespace exchange::binance::rest::test
