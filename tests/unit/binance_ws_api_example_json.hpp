// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Sample JSON frames from the Binance WebSocket API (trading) docs (see
// docs/plans/001-appendix-binance-message-formats.md §4, fetched 2026-06-06).
// Used as test fixtures in test_binance_ws_client.cpp.
//
// Fixtures are verbatim from the appendix unless marked synthetic.

#pragma once

namespace exchange::binance::ws::test {

// ─────────────────────────────────────────────────────────────────────────────
// ping reply (synthetic — docs shape; appendix §4 documents only order.place)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kWsApiPongJson = R"({"id":1,"status":200,"result":{}})";

// Synthetic string-id variant — Binance allows string or int request ids; the
// descriptor must pass string ids through verbatim.
inline constexpr const char* kWsApiStringIdPongJson =
    R"({"id":"e2a85d9f-07a5-4f94-8d5f-789dc3deb097","status":200,"result":{}})";

// ─────────────────────────────────────────────────────────────────────────────
// order.place replies (appendix §4 verbatim)
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr const char* kWsApiOrderPlaceSuccessJson =
    R"({"id":"e2a85d9f-07a5-4f94-8d5f-789dc3deb097","status":200,"result":{"symbol":"BTCUSDT","orderId":12510053279,"orderListId":-1,"clientOrderId":"a097fe6304b20a7e4fc436","transactTime":1655716096505,"price":"0.10000000","origQty":"10.00000000","executedQty":"0.00000000","origQuoteOrderQty":"0.000000","cummulativeQuoteQty":"0.00000000","status":"NEW","timeInForce":"GTC","type":"LIMIT","side":"BUY","workingTime":1655716096505,"selfTradePreventionMode":"NONE"},"rateLimits":[{"rateLimitType":"ORDERS","interval":"SECOND","intervalNum":10,"limit":50,"count":12}]})";

inline constexpr const char* kWsApiOrderPlaceErrorJson =
    R"({"id":"e2a85d9f-07a5-4f94-8d5f-789dc3deb097","status":400,"error":{"code":-2010,"msg":"Account has insufficient balance for requested action."},"rateLimits":[{"rateLimitType":"ORDERS","interval":"SECOND","intervalNum":10,"limit":50,"count":13}]})";

} // namespace exchange::binance::ws::test
