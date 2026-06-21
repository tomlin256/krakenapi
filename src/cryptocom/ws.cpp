// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/cryptocom/ws.hpp"

#include "exchange/cryptocom/heartbeat_connection.hpp"

#include <memory>
#include <string>
#include <utility>

namespace exchange::cryptocom::ws {

namespace {

// Monetary/size strings → double (tolerate a raw number); absent/null → 0.
double sd(const json& j, const char* key) {
    if (!j.is_object() || !j.contains(key) || j.at(key).is_null()) return 0.0;
    const auto& v = j.at(key);
    if (v.is_string()) return std::stod(v.get<std::string>());
    if (v.is_number()) return v.get<double>();
    return 0.0;
}

int64_t num_i64(const json& j, const char* key) {
    if (j.is_object() && j.contains(key) && j.at(key).is_number())
        return j.at(key).get<int64_t>();
    return 0;
}

int num_int(const json& j, const char* key) {
    if (j.is_object() && j.contains(key) && j.at(key).is_number())
        return j.at(key).get<int>();
    return 0;
}

std::string str_field(const json& j, const char* key) {
    if (j.is_object() && j.contains(key) && j.at(key).is_string())
        return j.at(key).get<std::string>();
    return {};
}

// result object of a frame (empty object if absent).
const json& result_obj(const json& frame) {
    static const json empty = json::object();
    if (frame.is_object() && frame.contains("result") && frame.at("result").is_object())
        return frame.at("result");
    return empty;
}

// result.data array of a frame (empty array if absent).
const json& result_data(const json& frame) {
    static const json empty = json::array();
    const json& r = result_obj(frame);
    if (r.contains("data") && r.at("data").is_array())
        return r.at("data");
    return empty;
}

} // namespace

// ── Channel-name helpers ──────────────────────────────────────────────────────

std::string ticker_channel(const std::string& instrument) { return "ticker." + instrument; }
std::string trade_channel(const std::string& instrument)  { return "trade." + instrument; }
std::string book_channel(const std::string& instrument, int depth) {
    return "book." + instrument + "." + std::to_string(depth);
}
std::string candlestick_channel(const std::string& period, const std::string& instrument) {
    return "candlestick." + period + "." + instrument;
}
std::string user_order_channel(const std::string& instrument) { return "user.order." + instrument; }

// ── Market push events ────────────────────────────────────────────────────────

CryptoComTickerEvent CryptoComTickerEvent::from_json(const json& frame) {
    CryptoComTickerEvent e;
    const json& r = result_obj(frame);
    e.subscription    = str_field(r, "subscription");
    e.instrument_name = str_field(r, "instrument_name");
    const json& data = result_data(frame);
    if (!data.empty()) {
        const json& d = data.front();
        if (e.instrument_name.empty()) e.instrument_name = str_field(d, "i");
        e.high          = sd(d, "h");
        e.low           = sd(d, "l");
        e.last          = sd(d, "a");
        e.change        = sd(d, "c");
        e.bid           = sd(d, "b");
        e.bid_size      = sd(d, "bs");
        e.ask           = sd(d, "k");
        e.ask_size      = sd(d, "ks");
        e.volume        = sd(d, "v");
        e.value         = sd(d, "vv");
        e.open_interest = sd(d, "oi");
        e.timestamp     = num_i64(d, "t");
    }
    return e;
}

CryptoComWsTrade CryptoComWsTrade::from_json(const json& row) {
    CryptoComWsTrade t;
    t.trade_id  = str_field(row, "d");
    t.timestamp = num_i64(row, "t");
    t.price     = sd(row, "p");
    t.quantity  = sd(row, "q");
    t.side      = str_field(row, "s");
    t.match_id  = str_field(row, "m");
    return t;
}

CryptoComTradeEvent CryptoComTradeEvent::from_json(const json& frame) {
    CryptoComTradeEvent e;
    const json& r = result_obj(frame);
    e.subscription    = str_field(r, "subscription");
    e.instrument_name = str_field(r, "instrument_name");
    for (const auto& row : result_data(frame))
        e.trades.push_back(CryptoComWsTrade::from_json(row));
    return e;
}

CryptoComWsBookLevel CryptoComWsBookLevel::from_json(const json& row) {
    CryptoComWsBookLevel lvl;
    if (row.is_array() && row.size() >= 2) {
        lvl.price = std::stod(row[0].get<std::string>());
        lvl.size  = std::stod(row[1].get<std::string>());
        if (row.size() >= 3 && row[2].is_string())
            lvl.num_orders = std::stoll(row[2].get<std::string>());
    }
    return lvl;
}

CryptoComBookEvent CryptoComBookEvent::from_json(const json& frame) {
    CryptoComBookEvent e;
    const json& r = result_obj(frame);
    e.subscription    = str_field(r, "subscription");
    e.instrument_name = str_field(r, "instrument_name");
    e.depth           = num_int(r, "depth");
    const json& data = result_data(frame);
    if (!data.empty()) {
        const json& d = data.front();
        e.t = num_i64(d, "t");
        e.u = num_i64(d, "u");
        if (d.contains("bids") && d.at("bids").is_array())
            for (const auto& row : d.at("bids")) e.bids.push_back(CryptoComWsBookLevel::from_json(row));
        if (d.contains("asks") && d.at("asks").is_array())
            for (const auto& row : d.at("asks")) e.asks.push_back(CryptoComWsBookLevel::from_json(row));
    }
    return e;
}

CryptoComWsCandle CryptoComWsCandle::from_json(const json& row) {
    CryptoComWsCandle c;
    c.t  = num_i64(row, "t");
    c.ut = num_i64(row, "ut");
    c.o  = sd(row, "o");
    c.h  = sd(row, "h");
    c.l  = sd(row, "l");
    c.c  = sd(row, "c");
    c.v  = sd(row, "v");
    return c;
}

CryptoComCandlestickEvent CryptoComCandlestickEvent::from_json(const json& frame) {
    CryptoComCandlestickEvent e;
    const json& r = result_obj(frame);
    e.subscription    = str_field(r, "subscription");
    e.instrument_name = str_field(r, "instrument_name");
    e.interval        = str_field(r, "interval");
    for (const auto& row : result_data(frame))
        e.candles.push_back(CryptoComWsCandle::from_json(row));
    return e;
}

// ── User push events ──────────────────────────────────────────────────────────

CryptoComOrderUpdate CryptoComOrderUpdate::from_json(const json& row) {
    CryptoComOrderUpdate o;
    o.order_id            = str_field(row, "order_id");
    o.client_oid          = str_field(row, "client_oid");
    o.instrument_name     = str_field(row, "instrument_name");
    o.status              = str_field(row, "status");
    o.side                = str_field(row, "side");
    o.order_type          = str_field(row, "order_type");
    o.quantity            = sd(row, "quantity");
    o.limit_price         = sd(row, "limit_price");
    o.cumulative_quantity = sd(row, "cumulative_quantity");
    o.avg_price           = sd(row, "avg_price");
    o.create_time         = num_i64(row, "create_time");
    o.update_time         = num_i64(row, "update_time");
    return o;
}

CryptoComUserOrderEvent CryptoComUserOrderEvent::from_json(const json& frame) {
    CryptoComUserOrderEvent e;
    e.subscription = str_field(result_obj(frame), "subscription");
    for (const auto& row : result_data(frame))
        e.orders.push_back(CryptoComOrderUpdate::from_json(row));
    return e;
}

CryptoComBalanceUpdate CryptoComBalanceUpdate::from_json(const json& row) {
    CryptoComBalanceUpdate b;
    b.instrument_name          = str_field(row, "instrument_name");
    b.total_available_balance  = sd(row, "total_available_balance");
    b.total_cash_balance       = sd(row, "total_cash_balance");
    return b;
}

CryptoComUserBalanceEvent CryptoComUserBalanceEvent::from_json(const json& frame) {
    CryptoComUserBalanceEvent e;
    e.subscription = str_field(result_obj(frame), "subscription");
    for (const auto& row : result_data(frame))
        e.balances.push_back(CryptoComBalanceUpdate::from_json(row));
    return e;
}

// ── Subscribe ack ─────────────────────────────────────────────────────────────

CryptoComSubscribeAck CryptoComSubscribeAck::from_json(const json& j) {
    CryptoComSubscribeAck a;
    const int code = (j.is_object() && j.contains("code") && j.at("code").is_number())
                         ? j.at("code").get<int>() : -1;
    a.success = (code == 0);
    if (!a.success)
        a.error = "code " + std::to_string(code);
    if (j.contains("id") && j.at("id").is_number())
        a.id = j.at("id").get<int64_t>();
    a.channel = str_field(j, "channel");
    a.subscription = str_field(result_obj(j), "subscription");
    if (a.channel.empty())
        a.channel = str_field(result_obj(j), "channel");
    return a;
}

// ── Subscribe request scaffold ────────────────────────────────────────────────

std::string CryptoComSubscribeBase::route_key() const { return channel; }

json CryptoComSubscribeBase::to_json() const {
    json j;
    j["id"]                 = req_id;
    j["method"]             = "subscribe";
    j["params"]["channels"] = json::array({channel});
    return j;
}

json CryptoComSubscribeBase::unsubscribe_json() const {
    json j;
    j["id"]                 = req_id;
    j["method"]             = "unsubscribe";
    j["params"]["channels"] = json::array({channel});
    return j;
}

// ── User-channel auth ─────────────────────────────────────────────────────────

CryptoComAuthResponse CryptoComAuthResponse::from_json(const json& j) {
    CryptoComAuthResponse a;
    const int code = (j.is_object() && j.contains("code") && j.at("code").is_number())
                         ? j.at("code").get<int>() : -1;
    a.success = (code == 0);
    if (!a.success)
        a.error = "code " + std::to_string(code);
    if (j.contains("id") && j.at("id").is_number())
        a.id = j.at("id").get<int64_t>();
    return a;
}

json CryptoComAuthRequest::to_json() const {
    const int64_t nonce = rest::make_nonce();
    json j;
    j["id"]      = req_id;
    j["method"]  = "public/auth";
    j["api_key"] = creds.api_key;
    j["nonce"]   = nonce;
    // public/auth signs method + id + api_key + nonce (no params).
    j["sig"]     = creds.sign("public/auth", req_id, json::object(), nonce);
    return j;
}

// ── Frame descriptor ──────────────────────────────────────────────────────────

FrameDescriptor cryptocom_frame_descriptor(const json& j) {
    FrameDescriptor d;
    if (!j.is_object()) return d;  // Unknown

    const std::string method = j.value("method", std::string{});

    // Heartbeats are answered by HeartbeatResponder and should never reach here.
    if (method == "public/heartbeat" || method == "public/respond-heartbeat")
        return d;  // Unknown

    const bool has_id = j.contains("id") && j.at("id").is_number();
    const int64_t id  = has_id ? j.at("id").get<int64_t>() : 0;

    // id == -1 marks a server-pushed update; route by result.subscription.
    if (method == "subscribe" && has_id && id == -1) {
        d.kind      = FrameKind::PushMessage;
        d.route_key = str_field(result_obj(j), "subscription");
        return d;
    }

    // Any other id-bearing frame (subscribe ack / public/auth) is a method
    // response correlated by id.
    if (has_id) {
        d.kind           = FrameKind::MethodResponse;
        d.correlation_id = std::to_string(id);
        return d;
    }

    return d;  // Unknown
}

// ── Client factories ──────────────────────────────────────────────────────────

namespace {

std::shared_ptr<ExchangeWsClient>
make_with_heartbeat(std::shared_ptr<IWsConnection> conn,
                    std::shared_ptr<IWsErrorHandler> error_handler) {
    auto hb = std::make_shared<HeartbeatResponder>(std::move(conn));
    return exchange::ws::make_exchange_ws_client(
        std::move(hb), cryptocom_frame_descriptor, std::move(error_handler));
}

} // namespace

std::shared_ptr<CryptoComMarketClient>
make_cryptocom_market_client(std::shared_ptr<IWsConnection>   conn,
                             std::shared_ptr<IWsErrorHandler>  error_handler) {
    return make_with_heartbeat(std::move(conn), std::move(error_handler));
}

std::shared_ptr<CryptoComUserClient>
make_cryptocom_user_client(std::shared_ptr<IWsConnection>   conn,
                           std::shared_ptr<IWsErrorHandler>  error_handler) {
    return make_with_heartbeat(std::move(conn), std::move(error_handler));
}

} // namespace exchange::cryptocom::ws
