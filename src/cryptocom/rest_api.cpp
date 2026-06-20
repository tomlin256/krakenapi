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
    if (j.contains("data") && j.at("data").is_array())
        for (const auto& e : j.at("data"))
            r.instruments.push_back(CryptoComInstrument::from_json(e));
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
    if (j.contains("data") && j.at("data").is_array())
        for (const auto& e : j.at("data"))
            r.tickers.push_back(CryptoComTicker::from_json(e));
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
        if (d.contains("bids") && d.at("bids").is_array())
            for (const auto& row : d.at("bids"))
                b.bids.push_back(CryptoComBookLevel::from_json(row));
        if (d.contains("asks") && d.at("asks").is_array())
            for (const auto& row : d.at("asks"))
                b.asks.push_back(CryptoComBookLevel::from_json(row));
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
    if (j.contains("data") && j.at("data").is_array())
        for (const auto& e : j.at("data"))
            r.candles.push_back(CryptoComCandle::from_json(e));
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
    if (j.contains("data") && j.at("data").is_array())
        for (const auto& e : j.at("data"))
            r.trades.push_back(CryptoComPublicTrade::from_json(e));
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

} // namespace exchange::cryptocom::rest
