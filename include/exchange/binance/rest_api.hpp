// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/rest_api.hpp
// Binance Spot REST API — request bases, response envelope, and endpoint types.
//
// Namespace: exchange::binance::rest
//
// Steps 5 and 6 add the concrete endpoint request/response types.
// This file defines the scaffolding those types build on:
//   - PublicRequest / PrivateRequest marker bases
//   - TypedPublicRequest<R> / TypedPrivateRequest<R> (compile-time response binding)
//   - parse_binance_response<T>() — maps (http_status, json) → RestResponse<T>

#include "exchange/common/rest.hpp"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace exchange::binance::rest {

using json = nlohmann::json;
using exchange::rest::HttpRequest;

// ── Request base classes ──────────────────────────────────────────────────────

// Marker base for Binance public requests (no authentication).
struct PublicRequest {
    virtual ~PublicRequest() = default;
    virtual HttpRequest build() const = 0;
};

// Marker base for Binance private requests (requires BinanceAuth).
// build() constructs the request WITHOUT auth; BinanceRestClient calls
// auth.sign() before dispatch.
struct PrivateRequest {
    virtual ~PrivateRequest() = default;
    virtual HttpRequest build() const = 0;
};

// Typed wrappers — link each request type to its response type at compile time.
template<typename R>
struct TypedPublicRequest : PublicRequest {
    using response_type = R;
};

template<typename R>
struct TypedPrivateRequest : PrivateRequest {
    using response_type = R;
};

// ── Response envelope ─────────────────────────────────────────────────────────
//
// Binance has no wrapping envelope — success response is the bare object/array.
// Error shape: {"code": <negative_int>, "msg": "<description>"}
// Success signal: HTTP 2xx AND no "code" field in the response body.

template<typename T>
exchange::rest::RestResponse<T>
parse_binance_response(int http_status, const json& j) {
    exchange::rest::RestResponse<T> resp;
    if (http_status < 400 && !(j.is_object() && j.contains("code"))) {
        resp.ok     = true;
        resp.result = T::from_json(j);
    } else {
        resp.ok = false;
        std::string msg;
        if (j.is_object() && j.contains("msg"))
            msg = j.value("msg", "unknown error");
        else
            msg = "HTTP " + std::to_string(http_status);
        resp.errors.push_back(std::move(msg));
    }
    return resp;
}

// ── Helpers ────────────────────────────────────────────────────────────────────

namespace detail {

// Builds the percent-encoded query value Binance expects for multi-symbol
// requests, e.g. ["BTCUSDT","ETHBTC"] -> "%5B%22BTCUSDT%22%2C%22ETHBTC%22%5D".
// Hand-escapes only `[`, `]`, `"`, `,` — sufficient because real Binance
// symbols are uppercase alphanumeric and need no other escaping. This is not
// a general-purpose URL encoder and is not shared with Kraken's
// detail::url_encode.
inline std::string symbols_query_value(const std::vector<std::string>& symbols) {
    std::string out = "%5B";
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i) out += "%2C";
        out += "%22";
        out += symbols[i];
        out += "%22";
    }
    out += "%5D";
    return out;
}

} // namespace detail

// ── GET /api/v3/ping ─────────────────────────────────────────────────────────

struct BinancePing {
    static BinancePing from_json(const json&) { return {}; }
};

struct BinancePingRequest : TypedPublicRequest<BinancePing> {
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/ping";
        return r;
    }
};

// ── GET /api/v3/time ─────────────────────────────────────────────────────────

struct BinanceServerTime {
    int64_t server_time{0};
    static BinanceServerTime from_json(const json& j) {
        BinanceServerTime t;
        t.server_time = j.value("serverTime", int64_t{0});
        return t;
    }
};

struct BinanceServerTimeRequest : TypedPublicRequest<BinanceServerTime> {
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/time";
        return r;
    }
};

// ── GET /api/v3/ticker/price ─────────────────────────────────────────────────

struct BinanceTickerPriceEntry {
    std::string symbol;
    double      price{0.0};
    static BinanceTickerPriceEntry from_json(const json& j) {
        BinanceTickerPriceEntry e;
        e.symbol = j.value("symbol", "");
        e.price  = std::stod(j.value("price", "0"));
        return e;
    }
};

struct BinanceTickerPrice {
    std::vector<BinanceTickerPriceEntry> entries;
    static BinanceTickerPrice from_json(const json& j) {
        BinanceTickerPrice t;
        if (j.is_array()) {
            for (const auto& el : j)
                t.entries.push_back(BinanceTickerPriceEntry::from_json(el));
        } else {
            t.entries.push_back(BinanceTickerPriceEntry::from_json(j));
        }
        return t;
    }
};

struct BinanceTickerPriceRequest : TypedPublicRequest<BinanceTickerPrice> {
    std::optional<std::string>              symbol;
    std::optional<std::vector<std::string>> symbols;

    // Prefers `symbol` if set, else `symbols`, else neither.
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/ticker/price";
        if (symbol) {
            r.query = "symbol=" + *symbol;
        } else if (symbols && !symbols->empty()) {
            r.query = "symbols=" + detail::symbols_query_value(*symbols);
        }
        return r;
    }
};

} // namespace exchange::binance::rest
