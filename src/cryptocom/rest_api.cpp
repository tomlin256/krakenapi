// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/cryptocom/rest_api.hpp"

#include <string>

namespace exchange::cryptocom::rest {

namespace {

// Crypto.com sends monetary/size fields as JSON strings; tolerate a raw number
// too. Absent/null → 0.
double sd(const json& j, const char* key) {
    if (!j.contains(key) || j.at(key).is_null()) return 0.0;
    const auto& v = j.at(key);
    if (v.is_string()) return std::stod(v.get<std::string>());
    if (v.is_number()) return v.get<double>();
    return 0.0;
}

int64_t num_i64(const json& j, const char* key) {
    if (j.contains(key) && j.at(key).is_number()) return j.at(key).get<int64_t>();
    return 0;
}

int num_int(const json& j, const char* key) {
    if (j.contains(key) && j.at(key).is_number()) return j.at(key).get<int>();
    return 0;
}

std::string str_field(const json& j, const char* key) {
    if (j.contains(key) && j.at(key).is_string()) return j.at(key).get<std::string>();
    return {};
}

std::string endpoint(const char* method) {
    return std::string(API_PREFIX) + method;
}

} // namespace

// ── public/get-instruments ───────────────────────────────────────────────────

CryptoComInstrument CryptoComInstrument::from_json(const json& j) {
    CryptoComInstrument x;
    x.symbol            = str_field(j, "symbol");
    x.inst_type         = str_field(j, "inst_type");
    x.display_name      = str_field(j, "display_name");
    x.base_ccy          = str_field(j, "base_ccy");
    x.quote_ccy         = str_field(j, "quote_ccy");
    x.quote_decimals    = num_int(j, "quote_decimals");
    x.quantity_decimals = num_int(j, "quantity_decimals");
    x.price_tick_size   = sd(j, "price_tick_size");
    x.qty_tick_size     = sd(j, "qty_tick_size");
    x.tradable          = j.value("tradable", false);
    return x;
}

CryptoComInstrumentsResult CryptoComInstrumentsResult::from_json(const json& j) {
    CryptoComInstrumentsResult r;
    if (j.contains("data") && j.at("data").is_array()) {
        r.instruments.reserve(j.at("data").size());
        for (const auto& e : j.at("data"))
            r.instruments.push_back(CryptoComInstrument::from_json(e));
    }
    return r;
}

HttpRequest CryptoComInstrumentsRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = endpoint("public/get-instruments");
    return r;
}

// ── public/get-tickers ────────────────────────────────────────────────────────

CryptoComTicker CryptoComTicker::from_json(const json& j) {
    CryptoComTicker x;
    x.instrument_name = str_field(j, "i");
    x.high            = sd(j, "h");
    x.low             = sd(j, "l");
    x.last            = sd(j, "a");
    x.volume          = sd(j, "v");
    x.value           = sd(j, "vv");
    x.change          = sd(j, "c");
    x.bid             = sd(j, "b");
    x.ask             = sd(j, "k");
    x.open_interest   = sd(j, "oi");
    x.timestamp       = num_i64(j, "t");
    return x;
}

CryptoComTickersResult CryptoComTickersResult::from_json(const json& j) {
    CryptoComTickersResult r;
    if (j.contains("data") && j.at("data").is_array()) {
        r.tickers.reserve(j.at("data").size());
        for (const auto& e : j.at("data"))
            r.tickers.push_back(CryptoComTicker::from_json(e));
    }
    return r;
}

HttpRequest CryptoComTickersRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = endpoint("public/get-tickers");
    if (instrument_name)
        r.query = "instrument_name=" + *instrument_name;
    return r;
}

// ── public/get-book ───────────────────────────────────────────────────────────

CryptoComBookLevel CryptoComBookLevel::from_json(const json& row) {
    CryptoComBookLevel lvl;
    if (row.is_array() && row.size() >= 2) {
        lvl.price = std::stod(row[0].get<std::string>());
        lvl.size  = std::stod(row[1].get<std::string>());
        if (row.size() >= 3 && row[2].is_string())
            lvl.num_orders = std::stoll(row[2].get<std::string>());
    }
    return lvl;
}

CryptoComOrderBook CryptoComOrderBook::from_json(const json& j) {
    CryptoComOrderBook b;
    b.instrument_name = str_field(j, "instrument_name");
    b.depth           = num_int(j, "depth");
    if (j.contains("data") && j.at("data").is_array() && !j.at("data").empty()) {
        const auto& d = j.at("data").front();
        b.t = num_i64(d, "t");
        if (d.contains("bids") && d.at("bids").is_array()) {
            b.bids.reserve(d.at("bids").size());
            for (const auto& row : d.at("bids"))
                b.bids.push_back(CryptoComBookLevel::from_json(row));
        }
        if (d.contains("asks") && d.at("asks").is_array()) {
            b.asks.reserve(d.at("asks").size());
            for (const auto& row : d.at("asks"))
                b.asks.push_back(CryptoComBookLevel::from_json(row));
        }
    }
    return b;
}

HttpRequest CryptoComOrderBookRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = endpoint("public/get-book");
    r.query  = "instrument_name=" + instrument_name;
    if (depth)
        r.query += "&depth=" + std::to_string(*depth);
    return r;
}

// ── public/get-candlestick ────────────────────────────────────────────────────

CryptoComCandle CryptoComCandle::from_json(const json& j) {
    CryptoComCandle c;
    c.t = num_i64(j, "t");
    c.o = sd(j, "o");
    c.h = sd(j, "h");
    c.l = sd(j, "l");
    c.c = sd(j, "c");
    c.v = sd(j, "v");
    return c;
}

CryptoComCandlesResult CryptoComCandlesResult::from_json(const json& j) {
    CryptoComCandlesResult r;
    r.instrument_name = str_field(j, "instrument_name");
    r.interval        = str_field(j, "interval");
    if (j.contains("data") && j.at("data").is_array()) {
        r.candles.reserve(j.at("data").size());
        for (const auto& e : j.at("data"))
            r.candles.push_back(CryptoComCandle::from_json(e));
    }
    return r;
}

HttpRequest CryptoComCandlesRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = endpoint("public/get-candlestick");
    r.query  = "instrument_name=" + instrument_name;
    if (timeframe)
        r.query += "&timeframe=" + *timeframe;
    if (count)
        r.query += "&count=" + std::to_string(*count);
    return r;
}

// ── public/get-trades ─────────────────────────────────────────────────────────

CryptoComPublicTrade CryptoComPublicTrade::from_json(const json& j) {
    CryptoComPublicTrade t;
    t.trade_id        = str_field(j, "d");
    t.timestamp       = num_i64(j, "t");
    t.quantity        = sd(j, "q");
    t.price           = sd(j, "p");
    t.side            = str_field(j, "s");
    t.instrument_name = str_field(j, "i");
    return t;
}

CryptoComTradesResult CryptoComTradesResult::from_json(const json& j) {
    CryptoComTradesResult r;
    if (j.contains("data") && j.at("data").is_array()) {
        r.trades.reserve(j.at("data").size());
        for (const auto& e : j.at("data"))
            r.trades.push_back(CryptoComPublicTrade::from_json(e));
    }
    return r;
}

HttpRequest CryptoComTradesRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = endpoint("public/get-trades");
    r.query  = "instrument_name=" + instrument_name;
    if (count)
        r.query += "&count=" + std::to_string(*count);
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// Private endpoints
// ═════════════════════════════════════════════════════════════════════════════

namespace {

// Assemble + sign the {id, method, api_key, params, nonce, sig} envelope and wrap
// it as a POST to /exchange/v1/<method>. params must already hold string values.
HttpRequest build_private(const CryptoComCredentials& creds, int64_t id,
                          const std::string& method, const json& params) {
    const int64_t nonce = make_nonce();

    json body;
    body["id"]      = id;
    body["method"]  = method;
    body["api_key"] = creds.api_key;
    body["params"]  = params;
    body["nonce"]   = nonce;
    body["sig"]     = creds.sign(method, id, params, nonce);

    HttpRequest r;
    r.method = HttpRequest::Method::POST;
    r.path   = std::string(API_PREFIX) + method;
    r.body   = body.dump();
    r.headers["Content-Type"] = "application/json";
    return r;
}

} // namespace

// ── Shared order record ───────────────────────────────────────────────────────

CryptoComOrder CryptoComOrder::from_json(const json& j) {
    CryptoComOrder o;
    o.account_id          = str_field(j, "account_id");
    o.order_id            = str_field(j, "order_id");
    o.client_oid          = str_field(j, "client_oid");
    o.order_type          = str_field(j, "order_type");
    o.time_in_force       = str_field(j, "time_in_force");
    o.side                = str_field(j, "side");
    o.quantity            = sd(j, "quantity");
    o.limit_price         = sd(j, "limit_price");
    o.order_value         = sd(j, "order_value");
    o.avg_price           = sd(j, "avg_price");
    o.cumulative_quantity = sd(j, "cumulative_quantity");
    o.cumulative_value    = sd(j, "cumulative_value");
    o.cumulative_fee      = sd(j, "cumulative_fee");
    o.status              = str_field(j, "status");
    o.status_enum         = cryptocom_order_status_from_string(o.status);
    o.create_time         = num_i64(j, "create_time");
    o.update_time         = num_i64(j, "update_time");
    o.instrument_name     = str_field(j, "instrument_name");
    o.fee_instrument_name = str_field(j, "fee_instrument_name");
    return o;
}

CryptoComOrdersResult CryptoComOrdersResult::from_json(const json& j) {
    CryptoComOrdersResult r;
    if (j.contains("data") && j.at("data").is_array()) {
        r.orders.reserve(j.at("data").size());
        for (const auto& e : j.at("data"))
            r.orders.push_back(CryptoComOrder::from_json(e));
    }
    return r;
}

// ── private/user-balance ──────────────────────────────────────────────────────

CryptoComPositionBalance CryptoComPositionBalance::from_json(const json& j) {
    CryptoComPositionBalance p;
    p.instrument_name        = str_field(j, "instrument_name");
    p.quantity               = sd(j, "quantity");
    p.market_value           = sd(j, "market_value");
    p.collateral_amount      = sd(j, "collateral_amount");
    p.max_withdrawal_balance = sd(j, "max_withdrawal_balance");
    p.reserved_qty           = sd(j, "reserved_qty");
    return p;
}

CryptoComUserBalance CryptoComUserBalance::from_json(const json& j) {
    CryptoComUserBalance b;
    b.total_available_balance     = sd(j, "total_available_balance");
    b.total_margin_balance        = sd(j, "total_margin_balance");
    b.total_initial_margin        = sd(j, "total_initial_margin");
    b.total_cash_balance          = sd(j, "total_cash_balance");
    b.total_collateral_value      = sd(j, "total_collateral_value");
    b.total_session_unrealized_pnl = sd(j, "total_session_unrealized_pnl");
    b.total_session_realized_pnl  = sd(j, "total_session_realized_pnl");
    b.instrument_name             = str_field(j, "instrument_name");
    if (j.contains("position_balances") && j.at("position_balances").is_array()) {
        b.position_balances.reserve(j.at("position_balances").size());
        for (const auto& e : j.at("position_balances"))
            b.position_balances.push_back(CryptoComPositionBalance::from_json(e));
    }
    return b;
}

CryptoComUserBalanceResult CryptoComUserBalanceResult::from_json(const json& j) {
    CryptoComUserBalanceResult r;
    if (j.contains("data") && j.at("data").is_array()) {
        r.data.reserve(j.at("data").size());
        for (const auto& e : j.at("data"))
            r.data.push_back(CryptoComUserBalance::from_json(e));
    }
    return r;
}

HttpRequest CryptoComUserBalanceRequest::build(const CryptoComCredentials& creds) const {
    return build_private(creds, id, "private/user-balance", json::object());
}

// ── private/create-order ──────────────────────────────────────────────────────

CryptoComCreateOrderResult CryptoComCreateOrderResult::from_json(const json& j) {
    CryptoComCreateOrderResult r;
    r.order_id   = str_field(j, "order_id");
    r.client_oid = str_field(j, "client_oid");
    return r;
}

HttpRequest CryptoComCreateOrderRequest::build(const CryptoComCredentials& creds) const {
    json params;
    params["instrument_name"] = instrument_name;
    params["side"]            = cryptocom_side_to_string(side);
    params["type"]            = cryptocom_order_type_to_string(type);
    if (price)         params["price"]         = *price;
    if (quantity)      params["quantity"]      = *quantity;
    if (notional)      params["notional"]      = *notional;
    if (client_oid)    params["client_oid"]    = *client_oid;
    if (time_in_force) params["time_in_force"] = cryptocom_tif_to_string(*time_in_force);
    if (exec_inst && !exec_inst->empty()) params["exec_inst"] = *exec_inst;
    return build_private(creds, id, "private/create-order", params);
}

// ── private/cancel-order and private/cancel-all-orders ────────────────────────

CryptoComCancelResult CryptoComCancelResult::from_json(const json&) {
    return CryptoComCancelResult{};
}

HttpRequest CryptoComCancelOrderRequest::build(const CryptoComCredentials& creds) const {
    json params;
    params["order_id"] = order_id;
    return build_private(creds, id, "private/cancel-order", params);
}

HttpRequest CryptoComCancelAllOrdersRequest::build(const CryptoComCredentials& creds) const {
    json params = json::object();
    if (instrument_name)
        params["instrument_name"] = *instrument_name;
    return build_private(creds, id, "private/cancel-all-orders", params);
}

// ── private/get-order-detail ──────────────────────────────────────────────────

HttpRequest CryptoComGetOrderDetailRequest::build(const CryptoComCredentials& creds) const {
    json params;
    params["order_id"] = order_id;
    return build_private(creds, id, "private/get-order-detail", params);
}

// ── private/get-open-orders ───────────────────────────────────────────────────

HttpRequest CryptoComGetOpenOrdersRequest::build(const CryptoComCredentials& creds) const {
    json params = json::object();
    if (instrument_name)
        params["instrument_name"] = *instrument_name;
    return build_private(creds, id, "private/get-open-orders", params);
}

// ── private/get-order-history ─────────────────────────────────────────────────

HttpRequest CryptoComGetOrderHistoryRequest::build(const CryptoComCredentials& creds) const {
    json params = json::object();
    if (instrument_name) params["instrument_name"] = *instrument_name;
    if (limit)           params["limit"]           = std::to_string(*limit);
    return build_private(creds, id, "private/get-order-history", params);
}

// ── private/get-trades ────────────────────────────────────────────────────────

CryptoComUserTrade CryptoComUserTrade::from_json(const json& j) {
    CryptoComUserTrade t;
    t.account_id          = str_field(j, "account_id");
    t.event_date          = str_field(j, "event_date");
    t.traded_quantity     = sd(j, "traded_quantity");
    t.traded_price        = sd(j, "traded_price");
    t.fees                = sd(j, "fees");
    t.order_id            = str_field(j, "order_id");
    t.trade_id            = str_field(j, "trade_id");
    t.trade_match_id      = str_field(j, "trade_match_id");
    t.create_time         = num_i64(j, "create_time");
    t.create_time_ns      = str_field(j, "create_time_ns");
    t.side                = str_field(j, "side");
    t.instrument_name     = str_field(j, "instrument_name");
    t.fee_instrument_name = str_field(j, "fee_instrument_name");
    t.client_oid          = str_field(j, "client_oid");
    t.taker_side          = str_field(j, "taker_side");
    t.liquidity_indicator = str_field(j, "liquidity_indicator");
    return t;
}

CryptoComUserTradesResult CryptoComUserTradesResult::from_json(const json& j) {
    CryptoComUserTradesResult r;
    if (j.contains("data") && j.at("data").is_array()) {
        r.trades.reserve(j.at("data").size());
        for (const auto& e : j.at("data"))
            r.trades.push_back(CryptoComUserTrade::from_json(e));
    }
    return r;
}

HttpRequest CryptoComGetTradesRequest::build(const CryptoComCredentials& creds) const {
    json params = json::object();
    if (instrument_name) params["instrument_name"] = *instrument_name;
    if (limit)           params["limit"]           = std::to_string(*limit);
    return build_private(creds, id, "private/get-trades", params);
}

} // namespace exchange::cryptocom::rest
