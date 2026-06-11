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

#include "exchange/binance/types.hpp"
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

// ── GET /api/v3/depth ────────────────────────────────────────────────────────

struct BinanceBookLevel {
    double price{0.0};
    double quantity{0.0};
    // Parses a positional 2-element row: ["price","qty"].
    static BinanceBookLevel from_json(const json& row) {
        BinanceBookLevel l;
        l.price    = std::stod(row.at(0).get<std::string>());
        l.quantity = std::stod(row.at(1).get<std::string>());
        return l;
    }
};

struct BinanceOrderBook {
    int64_t                       last_update_id{0};
    std::vector<BinanceBookLevel> bids;
    std::vector<BinanceBookLevel> asks;
    static BinanceOrderBook from_json(const json& j) {
        BinanceOrderBook b;
        b.last_update_id = j.value("lastUpdateId", int64_t{0});
        if (j.contains("bids"))
            for (const auto& row : j["bids"])
                b.bids.push_back(BinanceBookLevel::from_json(row));
        if (j.contains("asks"))
            for (const auto& row : j["asks"])
                b.asks.push_back(BinanceBookLevel::from_json(row));
        return b;
    }
};

struct BinanceOrderBookRequest : TypedPublicRequest<BinanceOrderBook> {
    std::string        symbol;
    std::optional<int> limit;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/depth";
        r.query  = "symbol=" + symbol;
        if (limit) r.query += "&limit=" + std::to_string(*limit);
        return r;
    }
};

// ── GET /api/v3/trades ───────────────────────────────────────────────────────

struct BinanceTrade {
    int64_t id{0};
    double  price{0.0};
    double  qty{0.0};
    double  quote_qty{0.0};
    int64_t time{0};
    bool    is_buyer_maker{false};
    bool    is_best_match{false};
    static BinanceTrade from_json(const json& j) {
        BinanceTrade t;
        t.id             = j.value("id", int64_t{0});
        t.price          = std::stod(j.value("price", "0"));
        t.qty            = std::stod(j.value("qty", "0"));
        t.quote_qty      = std::stod(j.value("quoteQty", "0"));
        t.time           = j.value("time", int64_t{0});
        t.is_buyer_maker = j.value("isBuyerMaker", false);
        t.is_best_match  = j.value("isBestMatch", false);
        return t;
    }
};

struct BinanceTradesResult {
    std::vector<BinanceTrade> trades;
    // Top-level array response.
    static BinanceTradesResult from_json(const json& j) {
        BinanceTradesResult r;
        for (const auto& el : j)
            r.trades.push_back(BinanceTrade::from_json(el));
        return r;
    }
};

struct BinanceRecentTradesRequest : TypedPublicRequest<BinanceTradesResult> {
    std::string        symbol;
    std::optional<int> limit;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/trades";
        r.query  = "symbol=" + symbol;
        if (limit) r.query += "&limit=" + std::to_string(*limit);
        return r;
    }
};

// ── GET /api/v3/klines ───────────────────────────────────────────────────────

struct BinanceKline {
    int64_t open_time{0};
    double  open{0.0};
    double  high{0.0};
    double  low{0.0};
    double  close{0.0};
    double  volume{0.0};
    int64_t close_time{0};
    double  quote_asset_volume{0.0};
    int64_t num_trades{0};
    double  taker_buy_base_volume{0.0};
    double  taker_buy_quote_volume{0.0};
    // Parses a 12-element positional row; field 11 ("ignore") is dropped.
    static BinanceKline from_json(const json& row) {
        BinanceKline k;
        k.open_time              = row.at(0).get<int64_t>();
        k.open                   = std::stod(row.at(1).get<std::string>());
        k.high                   = std::stod(row.at(2).get<std::string>());
        k.low                    = std::stod(row.at(3).get<std::string>());
        k.close                  = std::stod(row.at(4).get<std::string>());
        k.volume                 = std::stod(row.at(5).get<std::string>());
        k.close_time             = row.at(6).get<int64_t>();
        k.quote_asset_volume     = std::stod(row.at(7).get<std::string>());
        k.num_trades             = row.at(8).get<int64_t>();
        k.taker_buy_base_volume  = std::stod(row.at(9).get<std::string>());
        k.taker_buy_quote_volume = std::stod(row.at(10).get<std::string>());
        return k;
    }
};

struct BinanceKlinesResult {
    std::vector<BinanceKline> klines;
    // Top-level array response.
    static BinanceKlinesResult from_json(const json& j) {
        BinanceKlinesResult r;
        for (const auto& row : j)
            r.klines.push_back(BinanceKline::from_json(row));
        return r;
    }
};

struct BinanceKlinesRequest : TypedPublicRequest<BinanceKlinesResult> {
    std::string            symbol;
    std::string            interval;
    std::optional<int64_t> start_time;
    std::optional<int64_t> end_time;
    std::optional<int>     limit;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/klines";
        r.query  = "symbol=" + symbol + "&interval=" + interval;
        if (start_time) r.query += "&startTime=" + std::to_string(*start_time);
        if (end_time)   r.query += "&endTime=" + std::to_string(*end_time);
        if (limit)      r.query += "&limit=" + std::to_string(*limit);
        return r;
    }
};

// ── GET /api/v3/exchangeInfo ─────────────────────────────────────────────────

// First-cut scope: `filters`, `permissions`, `permissionSets`, `rateLimits`,
// `exchangeFilters`, and self-trade-prevention fields are deliberately
// omitted (see plan 003 design decision 6).
struct BinanceSymbolInfo {
    std::string              symbol;
    std::string              status;
    std::string              base_asset;
    int                      base_asset_precision{0};
    std::string              quote_asset;
    int                      quote_precision{0};
    int                      quote_asset_precision{0};
    std::vector<std::string> order_types;
    bool                     iceberg_allowed{false};
    bool                     oco_allowed{false};
    bool                     is_spot_trading_allowed{false};
    bool                     is_margin_trading_allowed{false};
    static BinanceSymbolInfo from_json(const json& j) {
        BinanceSymbolInfo s;
        s.symbol                    = j.value("symbol", "");
        s.status                    = j.value("status", "");
        s.base_asset                = j.value("baseAsset", "");
        s.base_asset_precision      = j.value("baseAssetPrecision", 0);
        s.quote_asset               = j.value("quoteAsset", "");
        s.quote_precision           = j.value("quotePrecision", 0);
        s.quote_asset_precision     = j.value("quoteAssetPrecision", 0);
        if (j.contains("orderTypes"))
            for (const auto& ot : j["orderTypes"])
                s.order_types.push_back(ot.get<std::string>());
        s.iceberg_allowed           = j.value("icebergAllowed", false);
        s.oco_allowed               = j.value("ocoAllowed", false);
        s.is_spot_trading_allowed   = j.value("isSpotTradingAllowed", false);
        s.is_margin_trading_allowed = j.value("isMarginTradingAllowed", false);
        return s;
    }
};

struct BinanceExchangeInfo {
    std::string                    timezone;
    int64_t                        server_time{0};
    std::vector<BinanceSymbolInfo> symbols;
    static BinanceExchangeInfo from_json(const json& j) {
        BinanceExchangeInfo e;
        e.timezone    = j.value("timezone", "");
        e.server_time = j.value("serverTime", int64_t{0});
        if (j.contains("symbols"))
            for (const auto& s : j["symbols"])
                e.symbols.push_back(BinanceSymbolInfo::from_json(s));
        return e;
    }
};

struct BinanceExchangeInfoRequest : TypedPublicRequest<BinanceExchangeInfo> {
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/exchangeInfo";
        return r;
    }
};

// ── GET /api/v3/ticker/24hr ──────────────────────────────────────────────────

struct BinanceTicker24hrEntry {
    std::string symbol;
    double      price_change{0.0};
    double      price_change_percent{0.0};
    double      weighted_avg_price{0.0};
    double      prev_close_price{0.0};
    double      last_price{0.0};
    double      last_qty{0.0};
    double      bid_price{0.0};
    double      bid_qty{0.0};
    double      ask_price{0.0};
    double      ask_qty{0.0};
    double      open_price{0.0};
    double      high_price{0.0};
    double      low_price{0.0};
    double      volume{0.0};
    double      quote_volume{0.0};
    int64_t     open_time{0};
    int64_t     close_time{0};
    int64_t     first_id{0};
    int64_t     last_id{0};
    int64_t     count{0};
    static BinanceTicker24hrEntry from_json(const json& j) {
        BinanceTicker24hrEntry e;
        e.symbol               = j.value("symbol", "");
        e.price_change         = std::stod(j.value("priceChange", "0"));
        e.price_change_percent = std::stod(j.value("priceChangePercent", "0"));
        e.weighted_avg_price   = std::stod(j.value("weightedAvgPrice", "0"));
        e.prev_close_price     = std::stod(j.value("prevClosePrice", "0"));
        e.last_price           = std::stod(j.value("lastPrice", "0"));
        e.last_qty             = std::stod(j.value("lastQty", "0"));
        e.bid_price            = std::stod(j.value("bidPrice", "0"));
        e.bid_qty              = std::stod(j.value("bidQty", "0"));
        e.ask_price            = std::stod(j.value("askPrice", "0"));
        e.ask_qty              = std::stod(j.value("askQty", "0"));
        e.open_price           = std::stod(j.value("openPrice", "0"));
        e.high_price           = std::stod(j.value("highPrice", "0"));
        e.low_price            = std::stod(j.value("lowPrice", "0"));
        e.volume               = std::stod(j.value("volume", "0"));
        e.quote_volume         = std::stod(j.value("quoteVolume", "0"));
        e.open_time            = j.value("openTime", int64_t{0});
        e.close_time           = j.value("closeTime", int64_t{0});
        e.first_id             = j.value("firstId", int64_t{0});
        e.last_id              = j.value("lastId", int64_t{0});
        e.count                = j.value("count", int64_t{0});
        return e;
    }
};

struct BinanceTicker24hr {
    std::vector<BinanceTicker24hrEntry> entries;
    // Single object (symbol= query) or array (symbols=/no query).
    static BinanceTicker24hr from_json(const json& j) {
        BinanceTicker24hr t;
        if (j.is_array()) {
            for (const auto& el : j)
                t.entries.push_back(BinanceTicker24hrEntry::from_json(el));
        } else {
            t.entries.push_back(BinanceTicker24hrEntry::from_json(j));
        }
        return t;
    }
};

struct BinanceTicker24hrRequest : TypedPublicRequest<BinanceTicker24hr> {
    std::optional<std::string>              symbol;
    std::optional<std::vector<std::string>> symbols;

    // Prefers `symbol` if set, else `symbols`, else neither.
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/ticker/24hr";
        if (symbol) {
            r.query = "symbol=" + *symbol;
        } else if (symbols && !symbols->empty()) {
            r.query = "symbols=" + detail::symbols_query_value(*symbols);
        }
        return r;
    }
};

} // namespace exchange::binance::rest
