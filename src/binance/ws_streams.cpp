// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/ws_streams.hpp"

#include <cctype>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace exchange::binance::ws {

// ── Payload unwrapping ────────────────────────────────────────────────────────

namespace detail {

const json& stream_payload(const json& j) {
    // NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter)
    return j.contains("data") ? j.at("data") : j;
}

// ASCII lowercase — Binance symbols are [A-Z0-9], so no locale concerns.
std::string lower(std::string s) {
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace detail

// ── Push event types ──────────────────────────────────────────────────────────

BinanceAggTradeEvent BinanceAggTradeEvent::from_json(const json& frame) {
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

BinanceTradeEvent BinanceTradeEvent::from_json(const json& frame) {
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

BinanceTickerEvent BinanceTickerEvent::from_json(const json& frame) {
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

BinanceMiniTickerEvent BinanceMiniTickerEvent::from_json(const json& frame) {
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

BinanceBookTickerEvent BinanceBookTickerEvent::from_json(const json& frame) {
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

BinanceStreamKline BinanceStreamKline::from_json(const json& k) {
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

BinanceKlineEvent BinanceKlineEvent::from_json(const json& frame) {
    const json& j = detail::stream_payload(frame);
    BinanceKlineEvent e;
    e.event_time = j.value("E", int64_t{0});
    e.symbol     = j.value("s", std::string{});
    if (j.contains("k"))
        e.kline = BinanceStreamKline::from_json(j.at("k"));
    return e;
}

BinanceDepthUpdateEvent BinanceDepthUpdateEvent::from_json(const json& frame) {
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

BinancePartialDepth BinancePartialDepth::from_json(const json& frame) {
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

// ── SUBSCRIBE/UNSUBSCRIBE ack ─────────────────────────────────────────────────

BinanceStreamAck BinanceStreamAck::from_json(const json& j) {
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

// ── Frame descriptor (MessageIdentifier) ──────────────────────────────────────

exchange::ws::FrameDescriptor
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

std::string agg_trade_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@aggTrade";
}
std::string trade_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@trade";
}
std::string kline_stream(const std::string& symbol, const std::string& interval) {
    return detail::lower(symbol) + "@kline_" + interval;
}
std::string ticker_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@ticker";
}
std::string mini_ticker_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@miniTicker";
}
std::string book_ticker_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@bookTicker";
}
std::string depth_stream(const std::string& symbol) {
    return detail::lower(symbol) + "@depth";
}
std::string partial_depth_stream(const std::string& symbol, int levels) {
    return detail::lower(symbol) + "@depth" + std::to_string(levels);
}

// ── Client factory ────────────────────────────────────────────────────────────

std::shared_ptr<BinanceStreamClient>
make_binance_stream_client(std::shared_ptr<IWsConnection>   conn,
                           std::shared_ptr<IWsErrorHandler>  error_handler) {
    return exchange::ws::make_exchange_ws_client(
        std::move(conn),
        binance_stream_frame_descriptor,
        std::move(error_handler));
}

} // namespace exchange::binance::ws
