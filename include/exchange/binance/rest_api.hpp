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

// Hoisted to exchange/binance/types.hpp (shared with the WS depth streams);
// re-exported here so existing exchange::binance::rest spellings keep resolving.
using exchange::binance::BinanceBookLevel;

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

// ═════════════════════════════════════════════════════════════════════════════
// Signed (private) endpoints — Step 6.
//
// build() constructs the request WITHOUT auth params; BinanceRestClient's
// execute(req, auth) calls auth.sign(), which appends timestamp/recvWindow/
// signature to the query (GET/DELETE) or body (POST) and signs the raw
// `query + body` concatenation. POST requests must therefore put ALL params
// in `body` and leave `query` empty; GET/DELETE use only `query`.
// ═════════════════════════════════════════════════════════════════════════════

// ── GET /api/v3/account ──────────────────────────────────────────────────────

struct BinanceCommissionRates {
    double maker{0.0};
    double taker{0.0};
    double buyer{0.0};
    double seller{0.0};

    static BinanceCommissionRates from_json(const json& j) {
        BinanceCommissionRates r;
        r.maker  = std::stod(j.value("maker", "0"));
        r.taker  = std::stod(j.value("taker", "0"));
        r.buyer  = std::stod(j.value("buyer", "0"));
        r.seller = std::stod(j.value("seller", "0"));
        return r;
    }
};

struct BinanceBalance {
    std::string asset;
    double      free{0.0};
    double      locked{0.0};

    static BinanceBalance from_json(const json& j) {
        BinanceBalance b;
        b.asset  = j.value("asset", "");
        b.free   = std::stod(j.value("free", "0"));
        b.locked = std::stod(j.value("locked", "0"));
        return b;
    }
};

struct BinanceAccount {
    int                         maker_commission{0};
    int                         taker_commission{0};
    int                         buyer_commission{0};
    int                         seller_commission{0};
    BinanceCommissionRates      commission_rates;
    bool                        can_trade{false};
    bool                        can_withdraw{false};
    bool                        can_deposit{false};
    bool                        brokered{false};
    bool                        require_self_trade_prevention{false};
    bool                        prevent_sor{false};
    int64_t                     update_time{0};
    std::string                 account_type;
    std::vector<BinanceBalance> balances;
    std::vector<std::string>    permissions;
    int64_t                     uid{0};

    static BinanceAccount from_json(const json& j) {
        BinanceAccount a;
        a.maker_commission  = j.value("makerCommission", 0);
        a.taker_commission  = j.value("takerCommission", 0);
        a.buyer_commission  = j.value("buyerCommission", 0);
        a.seller_commission = j.value("sellerCommission", 0);
        if (j.contains("commissionRates"))
            a.commission_rates = BinanceCommissionRates::from_json(j.at("commissionRates"));
        a.can_trade                     = j.value("canTrade", false);
        a.can_withdraw                  = j.value("canWithdraw", false);
        a.can_deposit                   = j.value("canDeposit", false);
        a.brokered                      = j.value("brokered", false);
        a.require_self_trade_prevention = j.value("requireSelfTradePrevention", false);
        a.prevent_sor                   = j.value("preventSor", false);
        a.update_time                   = j.value("updateTime", int64_t{0});
        a.account_type                  = j.value("accountType", "");
        for (const auto& el : j.value("balances", json::array()))
            a.balances.push_back(BinanceBalance::from_json(el));
        for (const auto& el : j.value("permissions", json::array()))
            a.permissions.push_back(el.get<std::string>());
        a.uid = j.value("uid", int64_t{0});
        return a;
    }
};

struct BinanceAccountRequest : TypedPrivateRequest<BinanceAccount> {
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/account";
        return r;
    }
};

// ── GET /api/v3/openOrders and /api/v3/allOrders ─────────────────────────────

// Shared row shape of both order-query endpoints.
//
// Enum fields follow plan 004 decision 3 ("enum where the wire↔enum mapping
// is total, raw string otherwise"): `status` folds unmapped values to
// OrderStatus::Unknown, `time_in_force`/`side` are total — but `type` stays
// a raw wire string because Binance's LIMIT_MAKER (and any future order
// type) has no canonical OrderType value to map to.
struct BinanceOrderInfo {
    std::string symbol;
    int64_t     order_id{0};
    int64_t     order_list_id{-1};
    std::string client_order_id;
    double      price{0.0};
    double      orig_qty{0.0};
    double      executed_qty{0.0};
    double      cummulative_quote_qty{0.0};  // wire: "cummulativeQuoteQty" (Binance's typo)
    double      orig_quote_order_qty{0.0};
    OrderStatus status{OrderStatus::Unknown};
    TimeInForce time_in_force{TimeInForce::GTC};
    std::string type;                        // raw wire string — see note above
    Side        side{Side::Buy};
    double      stop_price{0.0};
    double      iceberg_qty{0.0};
    int64_t     time{0};
    int64_t     update_time{0};
    bool        is_working{false};
    int64_t     working_time{0};
    std::string self_trade_prevention_mode;

    static BinanceOrderInfo from_json(const json& j) {
        BinanceOrderInfo o;
        o.symbol                = j.value("symbol", "");
        o.order_id              = j.value("orderId", int64_t{0});
        o.order_list_id         = j.value("orderListId", int64_t{-1});
        o.client_order_id       = j.value("clientOrderId", "");
        o.price                 = std::stod(j.value("price", "0"));
        o.orig_qty              = std::stod(j.value("origQty", "0"));
        o.executed_qty          = std::stod(j.value("executedQty", "0"));
        o.cummulative_quote_qty = std::stod(j.value("cummulativeQuoteQty", "0"));
        o.orig_quote_order_qty  = std::stod(j.value("origQuoteOrderQty", "0"));
        o.status                = binance_order_status_from_string(j.value("status", ""));
        o.time_in_force         = binance_tif_from_string(j.value("timeInForce", "GTC"));
        o.type                  = j.value("type", "");
        o.side                  = binance_side_from_string(j.value("side", "BUY"));
        o.stop_price            = std::stod(j.value("stopPrice", "0"));
        o.iceberg_qty           = std::stod(j.value("icebergQty", "0"));
        o.time                  = j.value("time", int64_t{0});
        o.update_time           = j.value("updateTime", int64_t{0});
        o.is_working            = j.value("isWorking", false);
        o.working_time          = j.value("workingTime", int64_t{0});
        o.self_trade_prevention_mode = j.value("selfTradePreventionMode", "");
        return o;
    }
};

struct BinanceOpenOrdersResult {
    std::vector<BinanceOrderInfo> orders;

    // Top-level array.
    static BinanceOpenOrdersResult from_json(const json& j) {
        BinanceOpenOrdersResult r;
        for (const auto& el : j)
            r.orders.push_back(BinanceOrderInfo::from_json(el));
        return r;
    }
};

// Same row shape and wrapper — distinct name per plan 001's endpoint table.
using BinanceAllOrdersResult = BinanceOpenOrdersResult;

struct BinanceOpenOrdersRequest : TypedPrivateRequest<BinanceOpenOrdersResult> {
    std::optional<std::string> symbol;  // omit -> open orders across all symbols

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/openOrders";
        if (symbol) r.query = "symbol=" + *symbol;
        return r;
    }
};

struct BinanceAllOrdersRequest : TypedPrivateRequest<BinanceAllOrdersResult> {
    std::string            symbol;      // required
    std::optional<int64_t> order_id;    // return orders >= this id
    std::optional<int64_t> start_time;
    std::optional<int64_t> end_time;
    std::optional<int>     limit;       // default 500, max 1000

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/allOrders";
        r.query  = "symbol=" + symbol;
        if (order_id)   r.query += "&orderId=" + std::to_string(*order_id);
        if (start_time) r.query += "&startTime=" + std::to_string(*start_time);
        if (end_time)   r.query += "&endTime=" + std::to_string(*end_time);
        if (limit)      r.query += "&limit=" + std::to_string(*limit);
        return r;
    }
};

// ── GET /api/v3/myTrades ─────────────────────────────────────────────────────

struct BinanceMyTrade {
    std::string symbol;
    int64_t     id{0};
    int64_t     order_id{0};
    int64_t     order_list_id{-1};
    double      price{0.0};
    double      qty{0.0};
    double      quote_qty{0.0};
    double      commission{0.0};
    std::string commission_asset;
    int64_t     time{0};
    bool        is_buyer{false};
    bool        is_maker{false};
    bool        is_best_match{false};

    static BinanceMyTrade from_json(const json& j) {
        BinanceMyTrade t;
        t.symbol           = j.value("symbol", "");
        t.id               = j.value("id", int64_t{0});
        t.order_id         = j.value("orderId", int64_t{0});
        t.order_list_id    = j.value("orderListId", int64_t{-1});
        t.price            = std::stod(j.value("price", "0"));
        t.qty              = std::stod(j.value("qty", "0"));
        t.quote_qty        = std::stod(j.value("quoteQty", "0"));
        t.commission       = std::stod(j.value("commission", "0"));
        t.commission_asset = j.value("commissionAsset", "");
        t.time             = j.value("time", int64_t{0});
        t.is_buyer         = j.value("isBuyer", false);
        t.is_maker         = j.value("isMaker", false);
        t.is_best_match    = j.value("isBestMatch", false);
        return t;
    }
};

struct BinanceMyTradesResult {
    std::vector<BinanceMyTrade> trades;

    // Top-level array.
    static BinanceMyTradesResult from_json(const json& j) {
        BinanceMyTradesResult r;
        for (const auto& el : j)
            r.trades.push_back(BinanceMyTrade::from_json(el));
        return r;
    }
};

struct BinanceMyTradesRequest : TypedPrivateRequest<BinanceMyTradesResult> {
    std::string            symbol;      // required
    std::optional<int64_t> order_id;
    std::optional<int64_t> start_time;
    std::optional<int64_t> end_time;
    std::optional<int64_t> from_id;     // trade id to fetch from
    std::optional<int>     limit;       // default 500, max 1000

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/api/v3/myTrades";
        r.query  = "symbol=" + symbol;
        if (order_id)   r.query += "&orderId=" + std::to_string(*order_id);
        if (start_time) r.query += "&startTime=" + std::to_string(*start_time);
        if (end_time)   r.query += "&endTime=" + std::to_string(*end_time);
        if (from_id)    r.query += "&fromId=" + std::to_string(*from_id);
        if (limit)      r.query += "&limit=" + std::to_string(*limit);
        return r;
    }
};

// ── POST /api/v3/order ───────────────────────────────────────────────────────

struct BinanceFill {
    double      price{0.0};
    double      qty{0.0};
    double      commission{0.0};
    std::string commission_asset;
    int64_t     trade_id{0};

    static BinanceFill from_json(const json& j) {
        BinanceFill f;
        f.price            = std::stod(j.value("price", "0"));
        f.qty              = std::stod(j.value("qty", "0"));
        f.commission       = std::stod(j.value("commission", "0"));
        f.commission_asset = j.value("commissionAsset", "");
        f.trade_id         = j.value("tradeId", int64_t{0});
        return f;
    }
};

// Covers all three newOrderRespType shapes: ACK fields are always present;
// RESULT/FULL fields are std::optional and only set when the server sent
// them; `fills` is non-empty only for FULL.
struct BinanceNewOrderResponse {
    // ACK and up:
    std::string symbol;
    int64_t     order_id{0};
    int64_t     order_list_id{-1};
    std::string client_order_id;
    int64_t     transact_time{0};
    // RESULT/FULL only:
    std::optional<double>      price;
    std::optional<double>      orig_qty;
    std::optional<double>      executed_qty;
    std::optional<double>      orig_quote_order_qty;
    std::optional<double>      cummulative_quote_qty;  // wire: "cummulativeQuoteQty"
    std::optional<OrderStatus> status;
    std::optional<TimeInForce> time_in_force;
    std::optional<std::string> type;                   // raw wire string
    std::optional<Side>        side;
    std::optional<int64_t>     working_time;
    std::optional<std::string> self_trade_prevention_mode;
    // FULL only (empty otherwise):
    std::vector<BinanceFill> fills;

    static BinanceNewOrderResponse from_json(const json& j) {
        BinanceNewOrderResponse r;
        r.symbol          = j.value("symbol", "");
        r.order_id        = j.value("orderId", int64_t{0});
        r.order_list_id   = j.value("orderListId", int64_t{-1});
        r.client_order_id = j.value("clientOrderId", "");
        r.transact_time   = j.value("transactTime", int64_t{0});
        if (j.contains("price"))
            r.price = std::stod(j.at("price").get<std::string>());
        if (j.contains("origQty"))
            r.orig_qty = std::stod(j.at("origQty").get<std::string>());
        if (j.contains("executedQty"))
            r.executed_qty = std::stod(j.at("executedQty").get<std::string>());
        if (j.contains("origQuoteOrderQty"))
            r.orig_quote_order_qty = std::stod(j.at("origQuoteOrderQty").get<std::string>());
        if (j.contains("cummulativeQuoteQty"))
            r.cummulative_quote_qty = std::stod(j.at("cummulativeQuoteQty").get<std::string>());
        if (j.contains("status"))
            r.status = binance_order_status_from_string(j.at("status").get<std::string>());
        if (j.contains("timeInForce"))
            r.time_in_force = binance_tif_from_string(j.at("timeInForce").get<std::string>());
        if (j.contains("type"))
            r.type = j.at("type").get<std::string>();
        if (j.contains("side"))
            r.side = binance_side_from_string(j.at("side").get<std::string>());
        if (j.contains("workingTime"))
            r.working_time = j.at("workingTime").get<int64_t>();
        if (j.contains("selfTradePreventionMode"))
            r.self_trade_prevention_mode = j.at("selfTradePreventionMode").get<std::string>();
        for (const auto& el : j.value("fills", json::array()))
            r.fills.push_back(BinanceFill::from_json(el));
        return r;
    }
};

struct BinanceNewOrderRequest : TypedPrivateRequest<BinanceNewOrderResponse> {
    std::string symbol;                              // required
    Side        side{Side::Buy};                     // required
    OrderType   type{OrderType::Limit};              // required
    std::optional<TimeInForce>          time_in_force;
    std::optional<std::string>          quantity;          // caller-formatted exact
    std::optional<std::string>          quote_order_qty;   //   decimals (plan 004
    std::optional<std::string>          price;             //   decision 6)
    std::optional<std::string>          new_client_order_id;
    std::optional<std::string>          stop_price;
    std::optional<std::string>          iceberg_qty;
    std::optional<BinanceOrderRespType> new_order_resp_type;

    // POST — all params go in the body (query stays empty; see the signed-
    // endpoint note above). Values are appended verbatim: valid Binance
    // symbols ([A-Z0-9]) and client order ids ([\.A-Z:/a-z0-9_-]{1,36})
    // contain no characters needing form-encoding — same minimal-escaping
    // stance as detail::symbols_query_value. Which params Binance *requires*
    // per order type (e.g. LIMIT needs timeInForce+quantity+price) is
    // enforced server-side; this struct stays a faithful wire mapping.
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::POST;
        r.path   = "/api/v3/order";
        r.body   = "symbol=" + symbol;
        r.body += "&side=" + exchange::binance::binance_side_to_string(side);
        r.body += "&type=" + exchange::binance::binance_order_type_to_string(type);
        if (time_in_force)
            r.body += "&timeInForce=" + exchange::binance::binance_tif_to_string(*time_in_force);
        if (quantity)            r.body += "&quantity=" + *quantity;
        if (quote_order_qty)     r.body += "&quoteOrderQty=" + *quote_order_qty;
        if (price)               r.body += "&price=" + *price;
        if (new_client_order_id) r.body += "&newClientOrderId=" + *new_client_order_id;
        if (stop_price)          r.body += "&stopPrice=" + *stop_price;
        if (iceberg_qty)         r.body += "&icebergQty=" + *iceberg_qty;
        if (new_order_resp_type)
            r.body += "&newOrderRespType="
                    + exchange::binance::binance_order_resp_type_to_string(*new_order_resp_type);
        return r;
    }
};

// ── DELETE /api/v3/order and /api/v3/openOrders ──────────────────────────────

// Shape shared by DELETE /api/v3/order's single object and each row of
// DELETE /api/v3/openOrders' array.
struct BinanceCancelOrderResponse {
    std::string symbol;
    std::string orig_client_order_id;
    int64_t     order_id{0};
    int64_t     order_list_id{-1};
    std::string client_order_id;
    int64_t     transact_time{0};
    double      price{0.0};
    double      orig_qty{0.0};
    double      executed_qty{0.0};
    double      orig_quote_order_qty{0.0};
    double      cummulative_quote_qty{0.0};  // wire: "cummulativeQuoteQty"
    OrderStatus status{OrderStatus::Unknown};
    TimeInForce time_in_force{TimeInForce::GTC};
    std::string type;                        // raw wire string
    Side        side{Side::Buy};
    std::string self_trade_prevention_mode;

    static BinanceCancelOrderResponse from_json(const json& j) {
        BinanceCancelOrderResponse r;
        r.symbol                = j.value("symbol", "");
        r.orig_client_order_id  = j.value("origClientOrderId", "");
        r.order_id              = j.value("orderId", int64_t{0});
        r.order_list_id         = j.value("orderListId", int64_t{-1});
        r.client_order_id       = j.value("clientOrderId", "");
        r.transact_time         = j.value("transactTime", int64_t{0});
        r.price                 = std::stod(j.value("price", "0"));
        r.orig_qty              = std::stod(j.value("origQty", "0"));
        r.executed_qty          = std::stod(j.value("executedQty", "0"));
        r.orig_quote_order_qty  = std::stod(j.value("origQuoteOrderQty", "0"));
        r.cummulative_quote_qty = std::stod(j.value("cummulativeQuoteQty", "0"));
        r.status                = binance_order_status_from_string(j.value("status", ""));
        r.time_in_force         = binance_tif_from_string(j.value("timeInForce", "GTC"));
        r.type                  = j.value("type", "");
        r.side                  = binance_side_from_string(j.value("side", "BUY"));
        r.self_trade_prevention_mode = j.value("selfTradePreventionMode", "");
        return r;
    }
};

// Note: DELETE /api/v3/openOrders can interleave OCO cancellation objects
// (contingencyType/orderReports) among plain rows; those parse as
// part-populated BinanceCancelOrderResponse rows (j.value defaults) — full
// OCO support is out of scope for the first cut (plan 004 decision 9).
struct BinanceCancelAllResponse {
    std::vector<BinanceCancelOrderResponse> orders;

    // Top-level array.
    static BinanceCancelAllResponse from_json(const json& j) {
        BinanceCancelAllResponse r;
        for (const auto& el : j)
            r.orders.push_back(BinanceCancelOrderResponse::from_json(el));
        return r;
    }
};

struct BinanceCancelOrderRequest : TypedPrivateRequest<BinanceCancelOrderResponse> {
    std::string                symbol;                // required
    std::optional<int64_t>     order_id;              // one of order_id /
    std::optional<std::string> orig_client_order_id;  //   orig_client_order_id
    std::optional<std::string> new_client_order_id;

    // DELETE — params go in the query (BinanceAuth signs query-side for
    // non-POST methods).
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::DELETE;
        r.path   = "/api/v3/order";
        r.query  = "symbol=" + symbol;
        if (order_id)
            r.query += "&orderId=" + std::to_string(*order_id);
        if (orig_client_order_id)
            r.query += "&origClientOrderId=" + *orig_client_order_id;
        if (new_client_order_id)
            r.query += "&newClientOrderId=" + *new_client_order_id;
        return r;
    }
};

struct BinanceCancelAllOpenOrdersRequest
    : TypedPrivateRequest<BinanceCancelAllResponse> {
    std::string symbol;  // required

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::DELETE;
        r.path   = "/api/v3/openOrders";
        r.query  = "symbol=" + symbol;
        return r;
    }
};

} // namespace exchange::binance::rest
