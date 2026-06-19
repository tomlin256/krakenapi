// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/coinbase/rest_api.hpp"

#include <string>

namespace exchange::coinbase::rest {

namespace {

// Append "key=value" to a query string, inserting '&' between pairs.
void add_param(std::string& q, const std::string& key, const std::string& value) {
    if (!q.empty()) q += '&';
    q += key + '=' + value;
}

// Parse a Coinbase string-encoded number ("1.5") with a default for absent keys.
double num(const json& j, const char* key) {
    return std::stod(j.value(key, std::string{"0"}));
}

} // namespace

// ── GET /time ────────────────────────────────────────────────────────────────

CoinbaseServerTime CoinbaseServerTime::from_json(const json& j) {
    CoinbaseServerTime t;
    t.iso   = j.value("iso", "");
    t.epoch = j.value("epoch", 0.0);
    return t;
}

HttpRequest CoinbaseServerTimeRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/time";
    return r;
}

// ── GET /products and /products/{id} ─────────────────────────────────────────

CoinbaseProduct CoinbaseProduct::from_json(const json& j) {
    CoinbaseProduct p;
    p.id               = j.value("id", "");
    p.display_name     = j.value("display_name", "");
    p.base_currency    = j.value("base_currency", "");
    p.quote_currency   = j.value("quote_currency", "");
    p.base_increment   = num(j, "base_increment");
    p.quote_increment  = num(j, "quote_increment");
    p.status           = j.value("status", "");
    p.trading_disabled = j.value("trading_disabled", false);
    return p;
}

CoinbaseProductsResult CoinbaseProductsResult::from_json(const json& j) {
    CoinbaseProductsResult r;
    for (const auto& row : j)
        r.products.push_back(CoinbaseProduct::from_json(row));
    return r;
}

HttpRequest CoinbaseProductsRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/products";
    return r;
}

HttpRequest CoinbaseProductRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/products/" + product_id;
    return r;
}

// ── GET /products/{id}/book ──────────────────────────────────────────────────

CoinbaseBookLevel CoinbaseBookLevel::from_json(const json& row) {
    CoinbaseBookLevel l;
    l.price = std::stod(row.at(0).get<std::string>());
    l.size  = std::stod(row.at(1).get<std::string>());
    // Level 1/2: third element is an order count. Level 3: an order-id string,
    // which is out of first-cut scope and leaves num_orders at 0.
    if (row.size() > 2 && row.at(2).is_number())
        l.num_orders = row.at(2).get<int64_t>();
    return l;
}

CoinbaseOrderBook CoinbaseOrderBook::from_json(const json& j) {
    CoinbaseOrderBook b;
    b.sequence = j.value("sequence", static_cast<int64_t>(0));
    if (j.contains("bids"))
        for (const auto& row : j.at("bids"))
            b.bids.push_back(CoinbaseBookLevel::from_json(row));
    if (j.contains("asks"))
        for (const auto& row : j.at("asks"))
            b.asks.push_back(CoinbaseBookLevel::from_json(row));
    return b;
}

HttpRequest CoinbaseOrderBookRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/products/" + product_id + "/book";
    if (level) add_param(r.query, "level", std::to_string(*level));
    return r;
}

// ── GET /products/{id}/ticker ────────────────────────────────────────────────

CoinbaseTicker CoinbaseTicker::from_json(const json& j) {
    CoinbaseTicker t;
    t.trade_id = j.value("trade_id", static_cast<int64_t>(0));
    t.price    = num(j, "price");
    t.size     = num(j, "size");
    t.bid      = num(j, "bid");
    t.ask      = num(j, "ask");
    t.volume   = num(j, "volume");
    t.time     = j.value("time", "");
    return t;
}

HttpRequest CoinbaseTickerRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/products/" + product_id + "/ticker";
    return r;
}

// ── GET /products/{id}/trades ────────────────────────────────────────────────

CoinbaseTrade CoinbaseTrade::from_json(const json& j) {
    CoinbaseTrade t;
    t.trade_id = j.value("trade_id", static_cast<int64_t>(0));
    t.time     = j.value("time", "");
    t.price    = num(j, "price");
    t.size     = num(j, "size");
    t.side     = j.value("side", "");
    return t;
}

CoinbaseTradesResult CoinbaseTradesResult::from_json(const json& j) {
    CoinbaseTradesResult r;
    for (const auto& row : j)
        r.trades.push_back(CoinbaseTrade::from_json(row));
    return r;
}

HttpRequest CoinbaseTradesRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/products/" + product_id + "/trades";
    if (limit) add_param(r.query, "limit", std::to_string(*limit));
    return r;
}

// ── GET /products/{id}/candles ───────────────────────────────────────────────

CoinbaseCandle CoinbaseCandle::from_json(const json& row) {
    CoinbaseCandle c;
    c.time   = row.at(0).get<int64_t>();
    c.low    = row.at(1).get<double>();
    c.high   = row.at(2).get<double>();
    c.open   = row.at(3).get<double>();
    c.close  = row.at(4).get<double>();
    c.volume = row.at(5).get<double>();
    return c;
}

CoinbaseCandlesResult CoinbaseCandlesResult::from_json(const json& j) {
    CoinbaseCandlesResult r;
    for (const auto& row : j)
        r.candles.push_back(CoinbaseCandle::from_json(row));
    return r;
}

HttpRequest CoinbaseCandlesRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/products/" + product_id + "/candles";
    if (granularity) add_param(r.query, "granularity", std::to_string(*granularity));
    if (start)       add_param(r.query, "start", *start);
    if (end)         add_param(r.query, "end", *end);
    return r;
}

// ── GET /products/{id}/stats ─────────────────────────────────────────────────

CoinbaseStats CoinbaseStats::from_json(const json& j) {
    CoinbaseStats s;
    s.open         = num(j, "open");
    s.high         = num(j, "high");
    s.low          = num(j, "low");
    s.last         = num(j, "last");
    s.volume       = num(j, "volume");
    s.volume_30day = num(j, "volume_30day");
    return s;
}

HttpRequest CoinbaseStatsRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/products/" + product_id + "/stats";
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// Private endpoints
// ═════════════════════════════════════════════════════════════════════════════

// ── /accounts ────────────────────────────────────────────────────────────────

CoinbaseAccount CoinbaseAccount::from_json(const json& j) {
    CoinbaseAccount a;
    a.id              = j.value("id", "");
    a.currency        = j.value("currency", "");
    a.balance         = num(j, "balance");
    a.hold            = num(j, "hold");
    a.available       = num(j, "available");
    a.profile_id      = j.value("profile_id", "");
    a.trading_enabled = j.value("trading_enabled", false);
    return a;
}

CoinbaseAccountsResult CoinbaseAccountsResult::from_json(const json& j) {
    CoinbaseAccountsResult r;
    for (const auto& row : j)
        r.accounts.push_back(CoinbaseAccount::from_json(row));
    return r;
}

HttpRequest CoinbaseAccountsRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/accounts";
    return r;
}

HttpRequest CoinbaseAccountRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/accounts/" + account_id;
    return r;
}

// ── Order record ─────────────────────────────────────────────────────────────

CoinbaseOrder CoinbaseOrder::from_json(const json& j) {
    CoinbaseOrder o;
    o.id             = j.value("id", "");
    o.product_id     = j.value("product_id", "");
    o.side           = j.value("side", "");
    o.type           = j.value("type", "");
    o.price          = num(j, "price");
    o.size           = num(j, "size");
    o.funds          = num(j, "funds");
    o.time_in_force  = j.value("time_in_force", "");
    o.post_only      = j.value("post_only", false);
    o.created_at     = j.value("created_at", "");
    o.fill_fees      = num(j, "fill_fees");
    o.filled_size    = num(j, "filled_size");
    o.executed_value = num(j, "executed_value");
    o.status         = j.value("status", "");
    o.status_enum    = coinbase_order_status_from_string(o.status);
    o.settled        = j.value("settled", false);
    o.done_reason    = j.value("done_reason", "");
    return o;
}

CoinbaseOrdersResult CoinbaseOrdersResult::from_json(const json& j) {
    CoinbaseOrdersResult r;
    for (const auto& row : j)
        r.orders.push_back(CoinbaseOrder::from_json(row));
    return r;
}

HttpRequest CoinbasePlaceOrderRequest::build() const {
    json body;
    body["product_id"] = product_id;
    body["side"]       = coinbase_side_to_string(side);
    body["type"]       = coinbase_order_type_to_string(type);
    if (client_oid)    body["client_oid"]    = *client_oid;
    if (price)         body["price"]         = *price;
    if (size)          body["size"]          = *size;
    if (funds)         body["funds"]         = *funds;
    if (time_in_force) body["time_in_force"] = coinbase_tif_to_string(*time_in_force);
    if (post_only)     body["post_only"]     = *post_only;
    if (cancel_after)  body["cancel_after"]  = *cancel_after;
    if (stop)          body["stop"]          = *stop;
    if (stop_price)    body["stop_price"]    = *stop_price;

    HttpRequest r;
    r.method = HttpRequest::Method::POST;
    r.path   = "/orders";
    r.body   = body.dump();
    return r;
}

HttpRequest CoinbaseGetOrderRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/orders/" + order_id;
    return r;
}

HttpRequest CoinbaseListOrdersRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/orders";
    if (status)     add_param(r.query, "status", *status);
    if (product_id) add_param(r.query, "product_id", *product_id);
    return r;
}

// ── Order cancellation ───────────────────────────────────────────────────────

CoinbaseCancelOrderResult CoinbaseCancelOrderResult::from_json(const json& j) {
    CoinbaseCancelOrderResult r;
    if (j.is_string())      r.order_id = j.get<std::string>();
    else if (j.is_object()) r.order_id = j.value("id", "");
    return r;
}

CoinbaseCancelAllResult CoinbaseCancelAllResult::from_json(const json& j) {
    CoinbaseCancelAllResult r;
    for (const auto& el : j)
        r.order_ids.push_back(el.get<std::string>());
    return r;
}

HttpRequest CoinbaseCancelOrderRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::DELETE;
    r.path   = "/orders/" + order_id;
    if (product_id) add_param(r.query, "product_id", *product_id);
    return r;
}

HttpRequest CoinbaseCancelAllOrdersRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::DELETE;
    r.path   = "/orders";
    if (product_id) add_param(r.query, "product_id", *product_id);
    return r;
}

// ── /fills ───────────────────────────────────────────────────────────────────

CoinbaseFill CoinbaseFill::from_json(const json& j) {
    CoinbaseFill f;
    f.trade_id   = j.value("trade_id", static_cast<int64_t>(0));
    f.product_id = j.value("product_id", "");
    f.order_id   = j.value("order_id", "");
    f.liquidity  = j.value("liquidity", "");
    f.price      = num(j, "price");
    f.size       = num(j, "size");
    f.fee        = num(j, "fee");
    f.created_at = j.value("created_at", "");
    f.side       = j.value("side", "");
    f.settled    = j.value("settled", false);
    return f;
}

CoinbaseFillsResult CoinbaseFillsResult::from_json(const json& j) {
    CoinbaseFillsResult r;
    for (const auto& row : j)
        r.fills.push_back(CoinbaseFill::from_json(row));
    return r;
}

HttpRequest CoinbaseFillsRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/fills";
    if (order_id)   add_param(r.query, "order_id", *order_id);
    if (product_id) add_param(r.query, "product_id", *product_id);
    return r;
}

} // namespace exchange::coinbase::rest
