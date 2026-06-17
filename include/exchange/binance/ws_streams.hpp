// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/ws_streams.hpp
// Binance WebSocket market streams — push event types, subscribe scaffold,
// frame descriptor, and client factory for the combined-stream endpoint.
// Member/function bodies are defined in src/binance/ws_streams.cpp (non-template)
// and binance/ws_streams.inl (template).
//
// Namespace: exchange::binance::ws
//
// This header does not include ix_ws_connection.hpp; for the real transport
// use the URL overload of exchange::ws::make_exchange_ws_client there.

#include "exchange/binance/types.hpp"
#include "exchange/common/ws_client.hpp"

#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace exchange::binance::ws {

using json = nlohmann::json;

// ── Re-export common client types ────────────────────────────────────────────

using exchange::ws::IWsConnection;
using exchange::ws::IWsErrorHandler;
using exchange::ws::RateLimitedWsErrorHandler;
using exchange::ws::WsResponse;
using exchange::ws::SubscriptionHandle;
using exchange::ws::ExchangeWsClient;
using exchange::ws::MessageIdentifier;
using exchange::ws::WsRequestBase;
using exchange::ws::FrameDescriptor;
using exchange::ws::FrameKind;
using exchange::ws::BaseWsResponse;

// ── Endpoint URL constant ─────────────────────────────────────────────────────

// Combined endpoint — every push frame arrives wrapped as
// {"stream":"<name>","data":{…}}, which is what the frame descriptor routes by.
inline constexpr std::string_view STREAM_URL = "wss://stream.binance.com/stream";

// ── Payload unwrapping (defined in src/binance/ws_streams.cpp) ───────────────

namespace detail {

// On the combined endpoint every event arrives as {"stream":…,"data":{…}};
// bare payloads have no wrapper. Returns a reference into the argument (the
// frame must outlive the returned reference).
const json& stream_payload(const json& j);

// ASCII lowercase — Binance symbols are [A-Z0-9], so no locale concerns.
std::string lower(std::string s);

} // namespace detail

// ── Push event types ──────────────────────────────────────────────────────────
//
// Wire shapes per appendix §3. Every from_json parses through
// detail::stream_payload, so both wrapped combined-endpoint frames and bare
// payloads are accepted. All from_json bodies are in src/binance/ws_streams.cpp.

// <symbol>@aggTrade
struct BinanceAggTradeEvent {
    int64_t     event_time{0};        // E
    std::string symbol;               // s
    int64_t     agg_trade_id{0};      // a
    double      price{0.0};           // p
    double      qty{0.0};             // q
    int64_t     first_trade_id{0};    // f
    int64_t     last_trade_id{0};     // l
    int64_t     trade_time{0};        // T
    bool        is_buyer_maker{false}; // m

    static BinanceAggTradeEvent from_json(const json& frame);
};

// <symbol>@trade
struct BinanceTradeEvent {
    int64_t     event_time{0};        // E
    std::string symbol;               // s
    int64_t     trade_id{0};          // t
    double      price{0.0};           // p
    double      qty{0.0};             // q
    int64_t     trade_time{0};        // T
    bool        is_buyer_maker{false}; // m

    static BinanceTradeEvent from_json(const json& frame);
};

// <symbol>@ticker — rolling 24 h statistics
struct BinanceTickerEvent {
    int64_t     event_time{0};            // E
    std::string symbol;                   // s
    double      price_change{0.0};        // p
    double      price_change_pct{0.0};    // P
    double      weighted_avg_price{0.0};  // w
    double      prev_close{0.0};          // x
    double      last_price{0.0};          // c
    double      last_qty{0.0};            // Q
    double      bid_price{0.0};           // b
    double      bid_qty{0.0};             // B
    double      ask_price{0.0};           // a
    double      ask_qty{0.0};             // A
    double      open{0.0};                // o
    double      high{0.0};                // h
    double      low{0.0};                 // l
    double      volume{0.0};              // v
    double      quote_volume{0.0};        // q
    int64_t     stats_open_time{0};       // O
    int64_t     stats_close_time{0};      // C
    int64_t     first_trade_id{0};        // F
    int64_t     last_trade_id{0};         // L
    int64_t     num_trades{0};            // n

    static BinanceTickerEvent from_json(const json& frame);
};

// <symbol>@miniTicker
struct BinanceMiniTickerEvent {
    int64_t     event_time{0};   // E
    std::string symbol;          // s
    double      close{0.0};      // c
    double      open{0.0};       // o
    double      high{0.0};       // h
    double      low{0.0};        // l
    double      volume{0.0};     // v
    double      quote_volume{0.0}; // q

    static BinanceMiniTickerEvent from_json(const json& frame);
};

// <symbol>@bookTicker — best bid/ask. The bare payload has no "e"/"E" event
// fields (appendix §3 note); on the combined endpoint identity comes from the
// wrapper's "stream" value.
struct BinanceBookTickerEvent {
    int64_t     update_id{0};  // u
    std::string symbol;        // s
    double      bid_price{0.0}; // b
    double      bid_qty{0.0};   // B
    double      ask_price{0.0}; // a
    double      ask_qty{0.0};   // A

    static BinanceBookTickerEvent from_json(const json& frame);
};

// <symbol>@kline_<interval> — candle payload nested under "k". Keyed
// single-letter fields; the REST kline is a positional 12-array — same data,
// different wire shape. Documented ignore field ("B") dropped.
struct BinanceStreamKline {
    int64_t     start_time{0};              // t
    int64_t     close_time{0};              // T
    std::string symbol;                     // s
    std::string interval;                   // i
    int64_t     first_trade_id{0};          // f
    int64_t     last_trade_id{0};           // L
    double      open{0.0};                  // o
    double      close{0.0};                 // c
    double      high{0.0};                  // h
    double      low{0.0};                   // l
    double      volume{0.0};                // v
    double      quote_volume{0.0};          // q
    double      taker_buy_base_volume{0.0}; // V
    double      taker_buy_quote_volume{0.0}; // Q
    int64_t     num_trades{0};              // n
    bool        is_closed{false};           // x

    // Parses the bare "k" object (BinanceKlineEvent extracts it).
    static BinanceStreamKline from_json(const json& k);
};

struct BinanceKlineEvent {
    int64_t            event_time{0}; // E
    std::string        symbol;        // s
    BinanceStreamKline kline;         // k

    static BinanceKlineEvent from_json(const json& frame);
};

// <symbol>@depth — differential book update.
struct BinanceDepthUpdateEvent {
    int64_t     event_time{0};      // E
    std::string symbol;             // s
    int64_t     first_update_id{0}; // U
    int64_t     final_update_id{0}; // u
    std::vector<BinanceBookLevel> bids; // b
    std::vector<BinanceBookLevel> asks; // a

    static BinanceDepthUpdateEvent from_json(const json& frame);
};

// <symbol>@depth<levels> — top-N snapshot. Same shape as the REST order book
// (full-name keys, no event envelope) but kept as its own type so ws code
// doesn't depend on rest_api.hpp.
struct BinancePartialDepth {
    int64_t last_update_id{0}; // lastUpdateId
    std::vector<BinanceBookLevel> bids;
    std::vector<BinanceBookLevel> asks;

    static BinancePartialDepth from_json(const json& frame);
};

// ── SUBSCRIBE/UNSUBSCRIBE ack ─────────────────────────────────────────────────

// Success: {"result":null,"id":N}. Failure: {"error":{"code":C,"msg":M},"id":N}.
// Deriving BaseWsResponse lets the generic detail::make_ws_response derive
// WsResponse::ok from success/error — no client change needed.
struct BinanceStreamAck : exchange::ws::BaseWsResponse {
    int64_t id{0};

    static BinanceStreamAck from_json(const json& j);
};

// ── Frame descriptor (MessageIdentifier) ──────────────────────────────────────
//
// Classifies combined-endpoint frames per appendix §5. Defined in
// src/binance/ws_streams.cpp.
exchange::ws::FrameDescriptor binance_stream_frame_descriptor(const json& j);

// ── Stream-name helpers (defined in src/binance/ws_streams.cpp) ──────────────
//
// Stream names require lowercase symbols with mixed-case suffixes
// ("btcusdt@aggTrade") while REST symbols are uppercase. The request structs
// store the stream string verbatim, so exotic streams stay reachable by
// passing a raw string; the helpers cover the documented defaults.

std::string agg_trade_stream(const std::string& symbol);
std::string trade_stream(const std::string& symbol);
std::string kline_stream(const std::string& symbol, const std::string& interval);
std::string ticker_stream(const std::string& symbol);
std::string mini_ticker_stream(const std::string& symbol);
std::string book_ticker_stream(const std::string& symbol);
std::string depth_stream(const std::string& symbol);
std::string partial_depth_stream(const std::string& symbol, int levels);

// ── Subscribe request scaffold ────────────────────────────────────────────────
//
// Satisfies ExchangeWsClient::subscribe_async's structural contract:
// WsRequestBase req_id slot, push_type/response_type, route_key(),
// to_json(), unsubscribe_json(). One stream per request. Member bodies are
// defined in binance/ws_streams.inl.

template<typename PushMsg>
struct TypedStreamSubscribeRequest : WsRequestBase {
    using push_type     = PushMsg;
    using response_type = BinanceStreamAck;

    std::string stream;  // e.g. "btcusdt@aggTrade" — see helpers above

    std::string route_key() const;
    json        to_json() const;
    json        unsubscribe_json() const;
};

using BinanceAggTradeSubscribe     = TypedStreamSubscribeRequest<BinanceAggTradeEvent>;
using BinanceTradeSubscribe        = TypedStreamSubscribeRequest<BinanceTradeEvent>;
using BinanceKlineSubscribe        = TypedStreamSubscribeRequest<BinanceKlineEvent>;
using BinanceTickerSubscribe       = TypedStreamSubscribeRequest<BinanceTickerEvent>;
using BinanceMiniTickerSubscribe   = TypedStreamSubscribeRequest<BinanceMiniTickerEvent>;
using BinanceBookTickerSubscribe   = TypedStreamSubscribeRequest<BinanceBookTickerEvent>;
using BinanceDepthSubscribe        = TypedStreamSubscribeRequest<BinanceDepthUpdateEvent>;
using BinancePartialDepthSubscribe = TypedStreamSubscribeRequest<BinancePartialDepth>;

// ── Client alias and connection-based factory ─────────────────────────────────
//
// BinanceStreamClient is ExchangeWsClient parameterised with the Binance
// stream frame descriptor — the same runtime type, like KrakenWsClient.
// For the real transport use the URL overload in
// exchange/common/ix_ws_connection.hpp:
//   exchange::ws::make_exchange_ws_client(std::string(STREAM_URL),
//                                         binance_stream_frame_descriptor);

using BinanceStreamClient = exchange::ws::ExchangeWsClient;

// Defined in src/binance/ws_streams.cpp.
std::shared_ptr<BinanceStreamClient>
make_binance_stream_client(std::shared_ptr<IWsConnection>   conn,
                           std::shared_ptr<IWsErrorHandler>  error_handler = nullptr);

} // namespace exchange::binance::ws

#include "exchange/binance/ws_streams.inl"
