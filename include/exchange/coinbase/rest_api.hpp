// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/coinbase/rest_api.hpp
// Coinbase Exchange REST API — request bases and endpoint request/response
// types. Bodies are defined in src/coinbase/rest_api.cpp; the response envelope
// helper parse_coinbase_response<T> lives in exchange/coinbase/types.hpp.
//
// Namespace: exchange::coinbase::rest
//
// Monetary / size fields arrive as JSON *strings* ("1.5") and are parsed with
// std::stod; candle rows are JSON numbers. requestPath (path + "?" + query) is
// what CoinbaseAuth signs, so public build()s set only path/query/body and
// private requests leave auth to the client (mirrors the Binance pattern).

#include "exchange/coinbase/types.hpp"
#include "exchange/common/rest.hpp"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace exchange::coinbase::rest {

using json = nlohmann::json;
using exchange::rest::HttpRequest;

// ── Request base classes ──────────────────────────────────────────────────────

// Marker base for Coinbase public requests (no authentication).
struct PublicRequest {
    virtual ~PublicRequest() = default;
    virtual HttpRequest build() const = 0;
};

// Marker base for Coinbase private requests. build() constructs the request
// WITHOUT auth; CoinbaseRestClient calls CoinbaseAuth::sign() before dispatch.
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

// ═════════════════════════════════════════════════════════════════════════════
// Public (unauthenticated) endpoints
// ═════════════════════════════════════════════════════════════════════════════

// ── GET /time ────────────────────────────────────────────────────────────────

struct CoinbaseServerTime {
    std::string iso;          // e.g. "2015-01-07T23:47:25.201Z"
    double      epoch{0.0};   // seconds since epoch (fractional)
    static CoinbaseServerTime from_json(const json& j);
};

struct CoinbaseServerTimeRequest : TypedPublicRequest<CoinbaseServerTime> {
    HttpRequest build() const override;
};

// ── GET /products and /products/{id} ─────────────────────────────────────────

struct CoinbaseProduct {
    std::string id;                 // e.g. "BTC-USD"
    std::string display_name;
    std::string base_currency;
    std::string quote_currency;
    double      base_increment{0.0};
    double      quote_increment{0.0};
    std::string status;             // "online", "offline", ...
    bool        trading_disabled{false};
    static CoinbaseProduct from_json(const json& j);
};

struct CoinbaseProductsResult {
    std::vector<CoinbaseProduct> products;
    // Top-level array.
    static CoinbaseProductsResult from_json(const json& j);
};

struct CoinbaseProductsRequest : TypedPublicRequest<CoinbaseProductsResult> {
    HttpRequest build() const override;
};

struct CoinbaseProductRequest : TypedPublicRequest<CoinbaseProduct> {
    std::string product_id;  // required, e.g. "BTC-USD"
    HttpRequest build() const override;
};

// ── GET /products/{id}/book ──────────────────────────────────────────────────
//
// First-cut scope: aggregated levels 1 & 2, whose rows are
// ["price","size",num_orders]. Level 3 (per-order ids) is out of scope — its
// third element is an order-id string and parses to num_orders = 0.

struct CoinbaseBookLevel {
    double  price{0.0};
    double  size{0.0};
    int64_t num_orders{0};
    static CoinbaseBookLevel from_json(const json& row);
};

struct CoinbaseOrderBook {
    int64_t                        sequence{0};
    std::vector<CoinbaseBookLevel> bids;
    std::vector<CoinbaseBookLevel> asks;
    static CoinbaseOrderBook from_json(const json& j);
};

struct CoinbaseOrderBookRequest : TypedPublicRequest<CoinbaseOrderBook> {
    std::string        product_id;     // required
    std::optional<int> level;          // 1 (best) or 2 (top 50 aggregated)
    HttpRequest build() const override;
};

// ── GET /products/{id}/ticker ────────────────────────────────────────────────

struct CoinbaseTicker {
    int64_t     trade_id{0};
    double      price{0.0};
    double      size{0.0};
    double      bid{0.0};
    double      ask{0.0};
    double      volume{0.0};
    std::string time;  // ISO 8601
    static CoinbaseTicker from_json(const json& j);
};

struct CoinbaseTickerRequest : TypedPublicRequest<CoinbaseTicker> {
    std::string product_id;  // required
    HttpRequest build() const override;
};

// ── GET /products/{id}/trades ────────────────────────────────────────────────

struct CoinbaseTrade {
    int64_t     trade_id{0};
    std::string time;       // ISO 8601
    double      price{0.0};
    double      size{0.0};
    std::string side;       // "buy" / "sell" (the maker side) — kept raw
    static CoinbaseTrade from_json(const json& j);
};

struct CoinbaseTradesResult {
    std::vector<CoinbaseTrade> trades;
    // Top-level array.
    static CoinbaseTradesResult from_json(const json& j);
};

struct CoinbaseTradesRequest : TypedPublicRequest<CoinbaseTradesResult> {
    std::string        product_id;  // required
    std::optional<int> limit;
    HttpRequest build() const override;
};

// ── GET /products/{id}/candles ───────────────────────────────────────────────
//
// Rows are positional JSON-number arrays: [time, low, high, open, close, volume].

struct CoinbaseCandle {
    int64_t time{0};  // bucket start, epoch seconds
    double  low{0.0};
    double  high{0.0};
    double  open{0.0};
    double  close{0.0};
    double  volume{0.0};
    static CoinbaseCandle from_json(const json& row);
};

struct CoinbaseCandlesResult {
    std::vector<CoinbaseCandle> candles;
    // Top-level array.
    static CoinbaseCandlesResult from_json(const json& j);
};

struct CoinbaseCandlesRequest : TypedPublicRequest<CoinbaseCandlesResult> {
    std::string                product_id;   // required
    std::optional<int>         granularity;  // 60/300/900/3600/21600/86400
    std::optional<std::string> start;        // ISO 8601
    std::optional<std::string> end;          // ISO 8601
    HttpRequest build() const override;
};

// ── GET /products/{id}/stats (24 h) ──────────────────────────────────────────

struct CoinbaseStats {
    double open{0.0};
    double high{0.0};
    double low{0.0};
    double last{0.0};
    double volume{0.0};
    double volume_30day{0.0};
    static CoinbaseStats from_json(const json& j);
};

struct CoinbaseStatsRequest : TypedPublicRequest<CoinbaseStats> {
    std::string product_id;  // required
    HttpRequest build() const override;
};

// ═════════════════════════════════════════════════════════════════════════════
// Private (authenticated) endpoints
//
// build() sets only method/path/query/body; CoinbaseRestClient::execute(req,
// creds) applies CoinbaseAuth::sign() (CB-ACCESS-* headers) before dispatch.
// Order price/size/funds are caller-formatted exact decimal strings — produce
// them with TickPrice::str() when exactness matters.
// ═════════════════════════════════════════════════════════════════════════════

// ── GET /accounts and /accounts/{id} ─────────────────────────────────────────

struct CoinbaseAccount {
    std::string id;
    std::string currency;
    double      balance{0.0};
    double      hold{0.0};
    double      available{0.0};
    std::string profile_id;
    bool        trading_enabled{false};
    static CoinbaseAccount from_json(const json& j);
};

struct CoinbaseAccountsResult {
    std::vector<CoinbaseAccount> accounts;
    // Top-level array.
    static CoinbaseAccountsResult from_json(const json& j);
};

struct CoinbaseAccountsRequest : TypedPrivateRequest<CoinbaseAccountsResult> {
    HttpRequest build() const override;
};

struct CoinbaseAccountRequest : TypedPrivateRequest<CoinbaseAccount> {
    std::string account_id;  // required
    HttpRequest build() const override;
};

// ── Order record (shared by place / get / list) ──────────────────────────────
//
// side/type/time_in_force are kept as raw wire strings (faithful mapping);
// status_enum is the convenience mapping of the raw `status` (Unknown
// fallback). When status == "done", done_reason distinguishes "filled" from
// "canceled".

struct CoinbaseOrder {
    std::string id;
    std::string product_id;
    std::string side;            // "buy" / "sell"
    std::string type;            // "limit" / "market"
    double      price{0.0};      // 0 for market orders
    double      size{0.0};
    double      funds{0.0};      // market-buy quote amount (0 otherwise)
    std::string time_in_force;   // "GTC" / "GTT" / "IOC" / "FOK"
    bool        post_only{false};
    std::string created_at;
    double      fill_fees{0.0};
    double      filled_size{0.0};
    double      executed_value{0.0};
    std::string status;          // raw wire status
    OrderStatus status_enum{OrderStatus::Unknown};
    bool        settled{false};
    std::string done_reason;     // "filled" / "canceled" when status == "done"
    static CoinbaseOrder from_json(const json& j);
};

struct CoinbaseOrdersResult {
    std::vector<CoinbaseOrder> orders;
    // Top-level array.
    static CoinbaseOrdersResult from_json(const json& j);
};

// ── POST /orders ─────────────────────────────────────────────────────────────

struct CoinbasePlaceOrderRequest : TypedPrivateRequest<CoinbaseOrder> {
    std::string product_id;                 // required
    Side        side{Side::Buy};            // required
    OrderType   type{OrderType::Limit};     // limit / market
    std::optional<std::string> client_oid;
    // limit:
    std::optional<std::string> price;          // exact decimal string
    std::optional<std::string> size;           // base size
    std::optional<TimeInForce> time_in_force;
    std::optional<bool>        post_only;
    std::optional<std::string> cancel_after;   // "min"/"hour"/"day" (GTT)
    // market:
    std::optional<std::string> funds;          // quote size (market buy)
    // stop overlay:
    std::optional<std::string> stop;           // "loss" / "entry"
    std::optional<std::string> stop_price;

    // Builds the JSON request body (POST /orders).
    HttpRequest build() const override;
};

// ── GET /orders and /orders/{id} ─────────────────────────────────────────────

struct CoinbaseGetOrderRequest : TypedPrivateRequest<CoinbaseOrder> {
    std::string order_id;  // required
    HttpRequest build() const override;
};

struct CoinbaseListOrdersRequest : TypedPrivateRequest<CoinbaseOrdersResult> {
    std::optional<std::string> status;      // "open"/"pending"/"active"/"done"/"all"
    std::optional<std::string> product_id;
    HttpRequest build() const override;
};

// ── DELETE /orders/{id} and DELETE /orders ───────────────────────────────────

// DELETE /orders/{id} returns the cancelled order id as a bare JSON string.
struct CoinbaseCancelOrderResult {
    std::string order_id;
    static CoinbaseCancelOrderResult from_json(const json& j);
};

// DELETE /orders returns a JSON array of cancelled order-id strings.
struct CoinbaseCancelAllResult {
    std::vector<std::string> order_ids;
    static CoinbaseCancelAllResult from_json(const json& j);
};

struct CoinbaseCancelOrderRequest : TypedPrivateRequest<CoinbaseCancelOrderResult> {
    std::string                order_id;     // required
    std::optional<std::string> product_id;   // optional filter
    HttpRequest build() const override;
};

struct CoinbaseCancelAllOrdersRequest : TypedPrivateRequest<CoinbaseCancelAllResult> {
    std::optional<std::string> product_id;   // optional filter
    HttpRequest build() const override;
};

// ── GET /fills ───────────────────────────────────────────────────────────────

struct CoinbaseFill {
    int64_t     trade_id{0};
    std::string product_id;
    std::string order_id;
    std::string liquidity;   // "M" (maker) / "T" (taker)
    double      price{0.0};
    double      size{0.0};
    double      fee{0.0};
    std::string created_at;
    std::string side;        // "buy" / "sell"
    bool        settled{false};
    static CoinbaseFill from_json(const json& j);
};

struct CoinbaseFillsResult {
    std::vector<CoinbaseFill> fills;
    // Top-level array.
    static CoinbaseFillsResult from_json(const json& j);
};

// Coinbase requires at least one of order_id / product_id.
struct CoinbaseFillsRequest : TypedPrivateRequest<CoinbaseFillsResult> {
    std::optional<std::string> order_id;
    std::optional<std::string> product_id;
    HttpRequest build() const override;
};

} // namespace exchange::coinbase::rest
