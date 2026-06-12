// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
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
//
// Namespace: exchange::binance::ws
//
// Built entirely on exchange::ws::ExchangeWsClient — Binance contributes only
// a MessageIdentifier (binance_stream_frame_descriptor), event payload types,
// the SUBSCRIBE/UNSUBSCRIBE request scaffold, and a factory. Wire shapes are
// documented in docs/plans/001-appendix-binance-message-formats.md §3/§5.
//
// This header does not include ix_ws_connection.hpp; for the real transport
// use the URL overload of exchange::ws::make_exchange_ws_client there.

#include "exchange/binance/types.hpp"
#include "exchange/common/ws_client.hpp"

#include <cctype>
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

// ── Payload unwrapping ────────────────────────────────────────────────────────

namespace detail {

// ExchangeWsClient hands the *whole* parsed frame to push callbacks, so on the
// combined endpoint every event arrives as {"stream":…,"data":{…}}. Bare
// payloads (documented fixtures, raw /ws single-stream use) have no wrapper.
// Every event from_json parses through this so both shapes are accepted.
//
// Returns a reference into the argument to avoid copying the payload subtree
// on every push frame. Lifetime contract: the frame must outlive the returned
// reference — every caller is an event from_json(const json&) using the result
// within its own body, where the frame is dispatch's lvalue.
inline const json& stream_payload(const json& j) {
    // NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter)
    return j.contains("data") ? j.at("data") : j;
}

// ASCII lowercase — Binance symbols are [A-Z0-9], so no locale concerns.
inline std::string lower(std::string s) {
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace detail

// ── Push event types ──────────────────────────────────────────────────────────
//
// Wire shapes per appendix §3. Terse single-letter keys map to named members;
// string-encoded numbers parse via std::stod (REST convention); ids/times are
// int64 ms. Documented "ignore" fields (aggTrade/trade "M") are dropped.
// Every from_json parses through detail::stream_payload, so both wrapped
// combined-endpoint frames and bare payloads are accepted.

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

    static BinanceAggTradeEvent from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinanceAggTradeEvent e;
        e.event_time     = j.value("E", int64_t{0});
        e.symbol         = j.value("s", std::string{});
        e.agg_trade_id   = j.value("a", int64_t{0});
        e.price          = std::stod(j.value("p", "0"));
        e.qty            = std::stod(j.value("q", "0"));
        e.first_trade_id = j.value("f", int64_t{0});
        e.last_trade_id  = j.value("l", int64_t{0});
        e.trade_time     = j.value("T", int64_t{0});
        e.is_buyer_maker = j.value("m", false);
        return e;
    }
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

    static BinanceTradeEvent from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinanceTradeEvent e;
        e.event_time     = j.value("E", int64_t{0});
        e.symbol         = j.value("s", std::string{});
        e.trade_id       = j.value("t", int64_t{0});
        e.price          = std::stod(j.value("p", "0"));
        e.qty            = std::stod(j.value("q", "0"));
        e.trade_time     = j.value("T", int64_t{0});
        e.is_buyer_maker = j.value("m", false);
        return e;
    }
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

    static BinanceTickerEvent from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinanceTickerEvent e;
        e.event_time         = j.value("E", int64_t{0});
        e.symbol             = j.value("s", std::string{});
        e.price_change       = std::stod(j.value("p", "0"));
        e.price_change_pct   = std::stod(j.value("P", "0"));
        e.weighted_avg_price = std::stod(j.value("w", "0"));
        e.prev_close         = std::stod(j.value("x", "0"));
        e.last_price         = std::stod(j.value("c", "0"));
        e.last_qty           = std::stod(j.value("Q", "0"));
        e.bid_price          = std::stod(j.value("b", "0"));
        e.bid_qty            = std::stod(j.value("B", "0"));
        e.ask_price          = std::stod(j.value("a", "0"));
        e.ask_qty            = std::stod(j.value("A", "0"));
        e.open               = std::stod(j.value("o", "0"));
        e.high               = std::stod(j.value("h", "0"));
        e.low                = std::stod(j.value("l", "0"));
        e.volume             = std::stod(j.value("v", "0"));
        e.quote_volume       = std::stod(j.value("q", "0"));
        e.stats_open_time    = j.value("O", int64_t{0});
        e.stats_close_time   = j.value("C", int64_t{0});
        e.first_trade_id     = j.value("F", int64_t{0});
        e.last_trade_id      = j.value("L", int64_t{0});
        e.num_trades         = j.value("n", int64_t{0});
        return e;
    }
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

    static BinanceMiniTickerEvent from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinanceMiniTickerEvent e;
        e.event_time   = j.value("E", int64_t{0});
        e.symbol       = j.value("s", std::string{});
        e.close        = std::stod(j.value("c", "0"));
        e.open         = std::stod(j.value("o", "0"));
        e.high         = std::stod(j.value("h", "0"));
        e.low          = std::stod(j.value("l", "0"));
        e.volume       = std::stod(j.value("v", "0"));
        e.quote_volume = std::stod(j.value("q", "0"));
        return e;
    }
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

    static BinanceBookTickerEvent from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinanceBookTickerEvent e;
        e.update_id = j.value("u", int64_t{0});
        e.symbol    = j.value("s", std::string{});
        e.bid_price = std::stod(j.value("b", "0"));
        e.bid_qty   = std::stod(j.value("B", "0"));
        e.ask_price = std::stod(j.value("a", "0"));
        e.ask_qty   = std::stod(j.value("A", "0"));
        return e;
    }
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
    static BinanceStreamKline from_json(const json& k) {
        BinanceStreamKline c;
        c.start_time             = k.value("t", int64_t{0});
        c.close_time             = k.value("T", int64_t{0});
        c.symbol                 = k.value("s", std::string{});
        c.interval               = k.value("i", std::string{});
        c.first_trade_id         = k.value("f", int64_t{0});
        c.last_trade_id          = k.value("L", int64_t{0});
        c.open                   = std::stod(k.value("o", "0"));
        c.close                  = std::stod(k.value("c", "0"));
        c.high                   = std::stod(k.value("h", "0"));
        c.low                    = std::stod(k.value("l", "0"));
        c.volume                 = std::stod(k.value("v", "0"));
        c.quote_volume           = std::stod(k.value("q", "0"));
        c.taker_buy_base_volume  = std::stod(k.value("V", "0"));
        c.taker_buy_quote_volume = std::stod(k.value("Q", "0"));
        c.num_trades             = k.value("n", int64_t{0});
        c.is_closed              = k.value("x", false);
        return c;
    }
};

struct BinanceKlineEvent {
    int64_t            event_time{0}; // E
    std::string        symbol;        // s
    BinanceStreamKline kline;         // k

    static BinanceKlineEvent from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinanceKlineEvent e;
        e.event_time = j.value("E", int64_t{0});
        e.symbol     = j.value("s", std::string{});
        if (j.contains("k"))
            e.kline = BinanceStreamKline::from_json(j.at("k"));
        return e;
    }
};

// <symbol>@depth — differential book update.
struct BinanceDepthUpdateEvent {
    int64_t     event_time{0};      // E
    std::string symbol;             // s
    int64_t     first_update_id{0}; // U
    int64_t     final_update_id{0}; // u
    std::vector<BinanceBookLevel> bids; // b
    std::vector<BinanceBookLevel> asks; // a

    static BinanceDepthUpdateEvent from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinanceDepthUpdateEvent e;
        e.event_time      = j.value("E", int64_t{0});
        e.symbol          = j.value("s", std::string{});
        e.first_update_id = j.value("U", int64_t{0});
        e.final_update_id = j.value("u", int64_t{0});
        if (j.contains("b"))
            for (const auto& row : j["b"])
                e.bids.push_back(BinanceBookLevel::from_json(row));
        if (j.contains("a"))
            for (const auto& row : j["a"])
                e.asks.push_back(BinanceBookLevel::from_json(row));
        return e;
    }
};

// <symbol>@depth<levels> — top-N snapshot. Same shape as the REST order book
// (full-name keys, no event envelope) but kept as its own type so ws code
// doesn't depend on rest_api.hpp.
struct BinancePartialDepth {
    int64_t last_update_id{0}; // lastUpdateId
    std::vector<BinanceBookLevel> bids;
    std::vector<BinanceBookLevel> asks;

    static BinancePartialDepth from_json(const json& frame) {
        const json& j = detail::stream_payload(frame);
        BinancePartialDepth d;
        d.last_update_id = j.value("lastUpdateId", int64_t{0});
        if (j.contains("bids"))
            for (const auto& row : j["bids"])
                d.bids.push_back(BinanceBookLevel::from_json(row));
        if (j.contains("asks"))
            for (const auto& row : j["asks"])
                d.asks.push_back(BinanceBookLevel::from_json(row));
        return d;
    }
};

// ── SUBSCRIBE/UNSUBSCRIBE ack ─────────────────────────────────────────────────

// Success: {"result":null,"id":N}. Failure: {"error":{"code":C,"msg":M},"id":N}.
// Deriving BaseWsResponse lets the generic detail::make_ws_response derive
// WsResponse::ok from success/error — no client change needed.
struct BinanceStreamAck : exchange::ws::BaseWsResponse {
    int64_t id{0};

    static BinanceStreamAck from_json(const json& j) {
        BinanceStreamAck a;
        if (j.contains("id") && !j.at("id").is_null())
            a.id = j.at("id").get<int64_t>();
        if (j.contains("error") && !j.at("error").is_null()) {
            a.success = false;
            a.error   = j.at("error").value("msg", "");
        } else {
            a.success = true;
        }
        return a;
    }
};

// ── Frame descriptor (MessageIdentifier) ──────────────────────────────────────

// Classifies combined-endpoint frames per appendix §5:
//   {"stream":"X@s","data":{…}}            → PushMessage, route_key = "X@s"
//   {"result"/"error":…, "id":N (non-null)} → MethodResponse, correlation_id = str(N)
//   anything else (incl. id:null)           → Unknown
//
// ExchangeWsClient keys pending handlers by std::to_string(req_id), so the
// correlation id here must stringify the integer id the same way.
inline exchange::ws::FrameDescriptor
binance_stream_frame_descriptor(const json& j) {
    exchange::ws::FrameDescriptor d;

    if (j.contains("stream")) {
        d.kind      = FrameKind::PushMessage;
        d.route_key = j.at("stream").get<std::string>();
        return d;
    }

    if (j.contains("id") && !j.at("id").is_null()
        && (j.contains("result") || j.contains("error"))) {
        const auto& id   = j.at("id");
        d.kind           = FrameKind::MethodResponse;
        d.correlation_id = id.is_string() ? id.get<std::string>()
                                          : std::to_string(id.get<int64_t>());
        return d;
    }

    return d;  // FrameKind::Unknown
}

// ── Stream-name helpers ───────────────────────────────────────────────────────
//
// Stream names require lowercase symbols with mixed-case suffixes
// ("btcusdt@aggTrade") while REST symbols are uppercase — a foot-gun these
// helpers own. The request structs store the stream string verbatim, so
// exotic streams ("@depth@100ms", "!ticker@arr") stay reachable by passing a
// raw string; the helpers cover the documented defaults. interval is a raw
// string, consistent with REST BinanceKlinesRequest.

inline std::string agg_trade_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@aggTrade";
}
inline std::string trade_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@trade";
}
inline std::string kline_stream(const std::string& symbol, const std::string& interval) {
    return detail::lower(symbol) + "@kline_" + interval;
}
inline std::string ticker_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@ticker";
}
inline std::string mini_ticker_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@miniTicker";
}
inline std::string book_ticker_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@bookTicker";
}
inline std::string depth_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@depth";
}
inline std::string partial_depth_stream(const std::string& symbol, int levels) {
    return detail::lower(symbol) + "@depth" + std::to_string(levels);
}

// ── Subscribe request scaffold ────────────────────────────────────────────────
//
// Satisfies ExchangeWsClient::subscribe_async's structural contract:
// WsRequestBase req_id slot, push_type/response_type, route_key(),
// to_json(), unsubscribe_json(). One stream per request — the params array
// always has exactly one element, matching the one-callback-per-route_key
// dispatch model (Binance allows batching, but a batched SUBSCRIBE gets a
// single ack with no per-stream failure signal — out of scope).

template<typename PushMsg>
struct TypedStreamSubscribeRequest : WsRequestBase {
    using push_type     = PushMsg;
    using response_type = BinanceStreamAck;

    std::string stream;  // e.g. "btcusdt@aggTrade" — see helpers above

    std::string route_key() const { return stream; }

    json to_json() const {
        return {{"method", "SUBSCRIBE"},
                {"params", json::array({stream})},
                {"id", req_id}};
    }

    // Pre-built by subscribe_async after req_id assignment; sent on cancel().
    json unsubscribe_json() const {
        return {{"method", "UNSUBSCRIBE"},
                {"params", json::array({stream})},
                {"id", req_id}};
    }
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

inline std::shared_ptr<BinanceStreamClient>
make_binance_stream_client(std::shared_ptr<IWsConnection>   conn,
                           std::shared_ptr<IWsErrorHandler>  error_handler = nullptr) {
    return exchange::ws::make_exchange_ws_client(
        std::move(conn),
        binance_stream_frame_descriptor,
        std::move(error_handler));
}

} // namespace exchange::binance::ws
