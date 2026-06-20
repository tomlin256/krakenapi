// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/cryptocom/types.hpp
// Crypto.com-specific enum converters and the REST response envelope helper.
//
// Namespace: exchange::cryptocom
//
// The canonical enums (Side, OrderType, TimeInForce, OrderStatus) live in
// exchange/common/types.hpp and are re-exported here. Crypto.com's wire formats
// are UPPERCASE words ("BUY", "LIMIT", "GOOD_TILL_CANCEL"), so — per the
// binance_*/coinbase_* precedent — Crypto.com keeps its own converter pair per
// enum.
//
// Conversion policy:
//   - Side        : total both ways ("BUY"/"SELL").
//   - OrderType   : maps the six Crypto.com order types (LIMIT/MARKET/STOP_LOSS/
//                   STOP_LIMIT/TAKE_PROFIT/TAKE_PROFIT_LIMIT); throws for the
//                   canonical values Crypto.com has no equivalent for (Iceberg,
//                   TrailingStop*, SettlePosition). Canonical StopLossLimit ↔
//                   Crypto.com "STOP_LIMIT".
//   - TimeInForce : GTC/IOC/FOK map to Crypto.com's long names; GTD throws
//                   (Crypto.com v1 has no good-till-date).
//   - OrderStatus : from_string is total via the Unknown fallback (REJECTED and
//                   any unmapped status fold to Unknown — read the raw status).

#include "exchange/common/rest.hpp"
#include "exchange/common/types.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace exchange::cryptocom {

using json = nlohmann::json;

// Re-export the canonical enums so exchange::cryptocom code refers to them
// unprefixed (mirrors exchange/binance/types.hpp).
using exchange::Side;
using exchange::OrderType;
using exchange::TimeInForce;
using exchange::OrderStatus;

// Converters below are defined in src/cryptocom/types.cpp.

std::string cryptocom_side_to_string(Side v);
Side        cryptocom_side_from_string(const std::string& s);

std::string cryptocom_order_type_to_string(OrderType v);
OrderType   cryptocom_order_type_from_string(const std::string& s);

std::string cryptocom_tif_to_string(TimeInForce v);
TimeInForce cryptocom_tif_from_string(const std::string& s);

OrderStatus cryptocom_order_status_from_string(const std::string& s);

// ── REST response envelope ─────────────────────────────────────────────────────
//
// Every Crypto.com response is {id, method, code, result}. code == 0 means
// success; a non-zero code (or a non-2xx HTTP status) is an error, usually with a
// "message" field. parse_cryptocom_response maps that onto the common
// RestResponse<T>. Defined inline (template); T::from_json(j) is resolved at the
// call site, where j is the inner `result` value.

template<typename T>
exchange::rest::RestResponse<T> parse_cryptocom_response(int http_status, const json& j) {
    exchange::rest::RestResponse<T> resp;

    // Crypto.com's code is always a numeric integer (0 = success). Anything
    // else leaves code at -1, which is treated as an error below.
    int code = -1;
    if (j.is_object() && j.contains("code") && j.at("code").is_number())
        code = j.at("code").get<int>();

    const bool http_ok = http_status >= 200 && http_status < 300;
    if (http_ok && code == 0) {
        resp.ok = true;
        if (j.is_object() && j.contains("result"))
            resp.result = T::from_json(j.at("result"));
        else
            resp.result = T::from_json(j);
    } else {
        resp.ok = false;
        std::string err;
        if (j.is_object() && j.contains("message") && j.at("message").is_string())
            err = "code " + std::to_string(code) + ": " + j.at("message").get<std::string>();
        else if (code != 0 && code != -1)
            err = "code " + std::to_string(code);
        else
            err = "HTTP " + std::to_string(http_status);
        resp.errors.push_back(err);
    }
    return resp;
}

} // namespace exchange::cryptocom
