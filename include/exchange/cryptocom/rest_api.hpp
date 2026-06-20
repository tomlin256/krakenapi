// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/cryptocom/rest_api.hpp
// Crypto.com Exchange v1 REST API — request bases and endpoint request/response
// types. Bodies are defined in src/cryptocom/rest_api.cpp; the response envelope
// helper parse_cryptocom_response<T> lives in exchange/cryptocom/types.hpp.
//
// Namespace: exchange::cryptocom::rest
//
// Crypto.com wraps every response as {id, method, code, result}; the client
// hands the inner `result` value to each T::from_json (parse_cryptocom_response
// unwraps it). Most list endpoints nest their rows under result.data.
//
// Wire-format notes (pinned from live captures, plan 020 step 3):
//   - Monetary / size / price fields arrive as JSON *strings* → std::stod.
//   - Millisecond timestamps arrive as JSON *numbers* → int64.
//   - Order-book rows are positional ["price","size","num_orders"] string arrays.
//   - Tickers use terse single-letter keys (i/h/l/a/v/vv/c/b/k/oi/t).
//
// Public GET requests need no signing (build() sets only method/path/query).

#include "exchange/common/rest.hpp"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace exchange::cryptocom::rest {

using json = nlohmann::json;
using exchange::rest::HttpRequest;

// Path prefix shared by every Crypto.com v1 REST endpoint.
inline constexpr const char* API_PREFIX = "/exchange/v1/";

// ── Request base classes ──────────────────────────────────────────────────────

// Marker base for Crypto.com public requests (no authentication).
struct PublicRequest {
    virtual ~PublicRequest() = default;
    virtual HttpRequest build() const = 0;
};

// Typed wrapper — links each request type to its response type at compile time.
template<typename R>
struct TypedPublicRequest : PublicRequest {
    using response_type = R;
};

// ═════════════════════════════════════════════════════════════════════════════
// Public (unauthenticated) endpoints
// ═════════════════════════════════════════════════════════════════════════════

// ── public/get-instruments ───────────────────────────────────────────────────

struct CryptoComInstrument {
    std::string symbol;          // "BTC_USD" (spot), "BTCUSD-PERP" (perp)
    std::string inst_type;       // "CCY_PAIR" (spot), "PERPETUAL_SWAP", ...
    std::string display_name;
    std::string base_ccy;
    std::string quote_ccy;
    int         quote_decimals{0};
    int         quantity_decimals{0};
    double      price_tick_size{0.0};
    double      qty_tick_size{0.0};
    bool        tradable{false};
    static CryptoComInstrument from_json(const json& j);
};

struct CryptoComInstrumentsResult {
    std::vector<CryptoComInstrument> instruments;  // result.data[]
    static CryptoComInstrumentsResult from_json(const json& j);
};

struct CryptoComInstrumentsRequest : TypedPublicRequest<CryptoComInstrumentsResult> {
    HttpRequest build() const override;
};

// ── public/get-tickers ────────────────────────────────────────────────────────

struct CryptoComTicker {
    std::string instrument_name;   // i
    double      high{0.0};         // h  (24h high)
    double      low{0.0};          // l  (24h low)
    double      last{0.0};         // a  (latest trade price)
    double      volume{0.0};       // v  (24h base volume)
    double      value{0.0};        // vv (24h quote value)
    double      change{0.0};       // c  (24h price change)
    double      bid{0.0};          // b  (best bid)
    double      ask{0.0};          // k  (best ask)
    double      open_interest{0.0};// oi
    int64_t     timestamp{0};      // t  (ms)
    static CryptoComTicker from_json(const json& j);
};

struct CryptoComTickersResult {
    std::vector<CryptoComTicker> tickers;  // result.data[]
    static CryptoComTickersResult from_json(const json& j);
};

struct CryptoComTickersRequest : TypedPublicRequest<CryptoComTickersResult> {
    std::optional<std::string> instrument_name;  // omit → every instrument
    HttpRequest build() const override;
};

// ── public/get-book ───────────────────────────────────────────────────────────

struct CryptoComBookLevel {
    double  price{0.0};
    double  size{0.0};
    int64_t num_orders{0};
    static CryptoComBookLevel from_json(const json& row);  // ["price","size","count"]
};

struct CryptoComOrderBook {
    std::string                     instrument_name;
    int                             depth{0};
    int64_t                         t{0};  // ms
    std::vector<CryptoComBookLevel> bids;
    std::vector<CryptoComBookLevel> asks;
    // result = {depth, instrument_name, data:[{bids, asks, t}]}.
    static CryptoComOrderBook from_json(const json& j);
};

struct CryptoComOrderBookRequest : TypedPublicRequest<CryptoComOrderBook> {
    std::string        instrument_name;  // required
    std::optional<int> depth;            // e.g. 10 / 50
    HttpRequest build() const override;
};

// ── public/get-candlestick ────────────────────────────────────────────────────

struct CryptoComCandle {
    int64_t t{0};   // bucket start, ms
    double  o{0.0};
    double  h{0.0};
    double  l{0.0};
    double  c{0.0};
    double  v{0.0};
    static CryptoComCandle from_json(const json& j);  // {o,h,l,c,v,t}
};

struct CryptoComCandlesResult {
    std::string                  instrument_name;
    std::string                  interval;
    std::vector<CryptoComCandle> candles;  // result.data[]
    static CryptoComCandlesResult from_json(const json& j);
};

struct CryptoComCandlesRequest : TypedPublicRequest<CryptoComCandlesResult> {
    std::string                instrument_name;  // required
    std::optional<std::string> timeframe;        // "1m","5m","1h","1D",...
    std::optional<int>         count;
    HttpRequest build() const override;
};

// ── public/get-trades ─────────────────────────────────────────────────────────

struct CryptoComPublicTrade {
    std::string trade_id;         // d
    int64_t     timestamp{0};     // t (ms)
    double      quantity{0.0};    // q
    double      price{0.0};       // p
    std::string side;             // s ("buy"/"sell") — kept raw (lowercase wire)
    std::string instrument_name;  // i
    static CryptoComPublicTrade from_json(const json& j);
};

struct CryptoComTradesResult {
    std::vector<CryptoComPublicTrade> trades;  // result.data[]
    static CryptoComTradesResult from_json(const json& j);
};

struct CryptoComTradesRequest : TypedPublicRequest<CryptoComTradesResult> {
    std::string        instrument_name;  // required
    std::optional<int> count;
    HttpRequest build() const override;
};

} // namespace exchange::cryptocom::rest
