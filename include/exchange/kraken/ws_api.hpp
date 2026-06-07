// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/kraken/ws_api.hpp
// Kraken WebSocket v2 API — request/response types and message identification.
//
// Namespace: exchange::kraken::ws

#include "exchange/common/ws.hpp"
#include "exchange/kraken/types.hpp"

#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace exchange::kraken::ws {

using json = nlohmann::json;

// ── Re-export common scaffold types ──────────────────────────────────────────

using exchange::ws::WsRequestBase;
using exchange::ws::WsResponse;
using exchange::ws::SubscriptionHandle;
using exchange::ws::FrameDescriptor;
using exchange::ws::FrameKind;
using exchange::ws::BaseWsResponse;

// Method-call typed request base (re-export from common scaffold).
template<typename R>
using TypedWsRequest = exchange::ws::TypedWsRequest<R>;

// ── Forward declarations ──────────────────────────────────────────────────────

struct AddOrderResponse;
struct AmendOrderResponse;
struct CancelOrderResponse;
struct CancelAllResponse;
struct CancelOnDisconnectResponse;
struct BatchAddResponse;
struct BatchCancelResponse;
struct EditOrderResponse;
struct PongMessage;

// ── Response base ─────────────────────────────────────────────────────────────

struct BaseResponse : exchange::ws::BaseWsResponse {
    std::string              method;
    std::optional<int64_t>   req_id;
    std::optional<std::string> time_in;
    std::optional<std::string> time_out;

    static void parse_base(const json& j, BaseResponse& r) {
        if (j.contains("method"))   r.method   = j["method"].get<std::string>();
        if (j.contains("success"))  r.success  = j["success"].get<bool>();
        if (j.contains("req_id"))   r.req_id   = j["req_id"].get<int64_t>();
        if (j.contains("error"))    r.error    = j["error"].get<std::string>();
        if (j.contains("time_in"))  r.time_in  = j["time_in"].get<std::string>();
        if (j.contains("time_out")) r.time_out = j["time_out"].get<std::string>();
    }
};

// ── Authentication credentials ────────────────────────────────────────────────

struct WsCredentials {
    std::string token;

    json to_json() const {
        json j;
        j["token"] = token;
        return j;
    }
};

// ── 1. ADD ORDER ──────────────────────────────────────────────────────────────

struct AddOrderRequest : TypedWsRequest<AddOrderResponse> {
    OrderType   order_type;
    Side        side;
    double      order_qty{0.0};
    std::string symbol;
    std::string token;

    std::optional<TickPrice> limit_price;
    std::optional<PriceType> limit_price_type;
    std::optional<Triggers>   triggers;
    std::optional<Conditional> conditional;
    std::optional<TimeInForce>   time_in_force;
    std::optional<bool>          margin;
    std::optional<bool>          post_only;
    std::optional<bool>          reduce_only;
    std::optional<std::string>   effective_time;
    std::optional<std::string>   expire_time;
    std::optional<std::string>   deadline;
    std::optional<std::string>   cl_ord_id;
    std::optional<int64_t>       order_userref;
    std::optional<double>        display_qty;
    std::optional<FeePreference> fee_preference;
    std::optional<StpType>       stp_type;
    std::optional<double>        cash_order_qty;
    std::optional<bool>          validate;
    std::optional<std::string>   sender_sub_id;

    json to_json() const {
        json params;
        params["order_type"] = kraken_order_type_to_string(order_type);
        params["side"]       = to_string(side);
        params["order_qty"]  = order_qty;
        params["symbol"]     = symbol;
        params["token"]      = token;

        if (limit_price)      params["limit_price"]      = limit_price->to_json();
        if (limit_price_type) params["limit_price_type"] = to_string(*limit_price_type);
        if (triggers)         params["triggers"]         = triggers->to_json();
        if (conditional)      params["conditional"]      = conditional->to_json();
        if (time_in_force)    params["time_in_force"]    = kraken_tif_to_string(*time_in_force);
        if (margin)           params["margin"]           = *margin;
        if (post_only)        params["post_only"]        = *post_only;
        if (reduce_only)      params["reduce_only"]      = *reduce_only;
        if (effective_time)   params["effective_time"]   = *effective_time;
        if (expire_time)      params["expire_time"]      = *expire_time;
        if (deadline)         params["deadline"]         = *deadline;
        if (cl_ord_id)        params["cl_ord_id"]        = *cl_ord_id;
        if (order_userref)    params["order_userref"]    = *order_userref;
        if (display_qty)      params["display_qty"]      = *display_qty;
        if (fee_preference)   params["fee_preference"]   = to_string(*fee_preference);
        if (stp_type)         params["stp_type"]         = to_string(*stp_type);
        if (cash_order_qty)   params["cash_order_qty"]   = *cash_order_qty;
        if (validate)         params["validate"]         = *validate;
        if (sender_sub_id)    params["sender_sub_id"]    = *sender_sub_id;

        json msg;
        msg["method"] = "add_order";
        msg["params"] = params;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct AddOrderResponse : BaseResponse {
    std::optional<std::string> order_id;
    std::optional<std::string> cl_ord_id;
    std::optional<int64_t>     order_userref;
    std::optional<std::vector<std::string>> warnings;

    static AddOrderResponse from_json(const json& j) {
        AddOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("order_id"))      r.order_id      = res["order_id"].get<std::string>();
            if (res.contains("cl_ord_id"))     r.cl_ord_id     = res["cl_ord_id"].get<std::string>();
            if (res.contains("order_userref")) r.order_userref = res["order_userref"].get<int64_t>();
            if (res.contains("warnings"))      r.warnings      = res["warnings"].get<std::vector<std::string>>();
        }
        return r;
    }
};

// ── 2. AMEND ORDER ───────────────────────────────────────────────────────────

struct AmendOrderRequest : TypedWsRequest<AmendOrderResponse> {
    std::string token;
    std::optional<std::string> order_id;
    std::optional<std::string> cl_ord_id;
    std::optional<double>    order_qty;
    std::optional<double>    display_qty;
    std::optional<TickPrice> limit_price;
    std::optional<PriceType> limit_price_type;
    std::optional<Triggers>  triggers;
    std::optional<TickPrice> post_only_price;
    std::optional<std::string> deadline;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_id)         params["order_id"]         = *order_id;
        if (cl_ord_id)        params["cl_ord_id"]        = *cl_ord_id;
        if (order_qty)        params["order_qty"]        = *order_qty;
        if (display_qty)      params["display_qty"]      = *display_qty;
        if (limit_price)      params["limit_price"]      = limit_price->to_json();
        if (limit_price_type) params["limit_price_type"] = to_string(*limit_price_type);
        if (triggers)         params["triggers"]         = triggers->to_json();
        if (post_only_price)  params["post_only_price"]  = post_only_price->to_json();
        if (deadline)         params["deadline"]         = *deadline;

        json msg;
        msg["method"] = "amend_order";
        msg["params"] = params;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct AmendOrderResponse : BaseResponse {
    std::optional<std::string> order_id;
    std::optional<std::string> cl_ord_id;
    std::optional<std::vector<std::string>> warnings;

    static AmendOrderResponse from_json(const json& j) {
        AmendOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("order_id"))  r.order_id  = res["order_id"].get<std::string>();
            if (res.contains("cl_ord_id")) r.cl_ord_id = res["cl_ord_id"].get<std::string>();
            if (res.contains("warnings"))  r.warnings  = res["warnings"].get<std::vector<std::string>>();
        }
        return r;
    }
};

// ── 3. CANCEL ORDER ───────────────────────────────────────────────────────────

struct CancelOrderRequest : TypedWsRequest<CancelOrderResponse> {
    std::string token;
    std::optional<std::vector<std::string>> order_ids;
    std::optional<std::vector<std::string>> cl_ord_ids;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_ids && !order_ids->empty())   params["order_id"]  = *order_ids;
        if (cl_ord_ids && !cl_ord_ids->empty()) params["cl_ord_id"] = *cl_ord_ids;

        json msg;
        msg["method"] = "cancel_order";
        msg["params"] = params;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct CancelOrderResult {
    std::string order_id;
    bool        success{false};
    std::optional<std::string> error;
};

struct CancelOrderResponse : BaseResponse {
    std::optional<std::vector<CancelOrderResult>> orders_cancelled;

    static CancelOrderResponse from_json(const json& j) {
        CancelOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("orders_cancelled")) {
                std::vector<CancelOrderResult> v;
                for (const auto& item : res["orders_cancelled"]) {
                    CancelOrderResult cr;
                    cr.order_id = item.at("order_id").get<std::string>();
                    cr.success  = item.value("success", false);
                    if (item.contains("error")) cr.error = item["error"].get<std::string>();
                    v.push_back(cr);
                }
                r.orders_cancelled = v;
            }
        }
        return r;
    }
};

// ── 4. CANCEL ALL ─────────────────────────────────────────────────────────────

struct CancelAllRequest : TypedWsRequest<CancelAllResponse> {
    std::string token;

    json to_json() const {
        json msg;
        msg["method"] = "cancel_all";
        msg["params"]["token"] = token;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct CancelAllResponse : BaseResponse {
    std::optional<int32_t> count;

    static CancelAllResponse from_json(const json& j) {
        CancelAllResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("count")) r.count = res["count"].get<int32_t>();
        }
        return r;
    }
};

// ── 5. CANCEL ON DISCONNECT ───────────────────────────────────────────────────

struct CancelOnDisconnectRequest : TypedWsRequest<CancelOnDisconnectResponse> {
    std::string token;
    int32_t     timeout{60};

    json to_json() const {
        json msg;
        msg["method"] = "cancel_after";
        msg["params"]["token"]   = token;
        msg["params"]["timeout"] = timeout;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct CancelOnDisconnectResponse : BaseResponse {
    std::optional<std::string> current_time;
    std::optional<std::string> trigger_time;

    static CancelOnDisconnectResponse from_json(const json& j) {
        CancelOnDisconnectResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("current_time")) r.current_time = res["current_time"].get<std::string>();
            if (res.contains("trigger_time")) r.trigger_time = res["trigger_time"].get<std::string>();
        }
        return r;
    }
};

// ── 6. BATCH ADD ──────────────────────────────────────────────────────────────

struct BatchAddRequest : TypedWsRequest<BatchAddResponse> {
    std::string token;
    std::string symbol;
    std::optional<std::string> deadline;
    std::optional<bool>        validate;
    std::vector<OrderParams> orders;

    json to_json() const {
        json params;
        params["token"]  = token;
        params["symbol"] = symbol;
        if (deadline) params["deadline"] = *deadline;
        if (validate) params["validate"] = *validate;

        json arr = json::array();
        for (const auto& o : orders) arr.push_back(o.to_json());
        params["orders"] = arr;

        json msg;
        msg["method"] = "batch_add";
        msg["params"] = params;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct BatchAddResult {
    std::string              order_id;
    bool                     success{false};
    std::optional<std::string> cl_ord_id;
    std::optional<int64_t>   order_userref;
    std::optional<std::string> error;
    std::optional<std::vector<std::string>> warnings;
};

struct BatchAddResponse : BaseResponse {
    std::optional<std::vector<BatchAddResult>> orders;

    static BatchAddResponse from_json(const json& j) {
        BatchAddResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("orders")) {
                std::vector<BatchAddResult> v;
                for (const auto& item : res["orders"]) {
                    BatchAddResult br;
                    br.success  = item.value("success", false);
                    if (item.contains("order_id"))      br.order_id      = item["order_id"].get<std::string>();
                    if (item.contains("cl_ord_id"))     br.cl_ord_id     = item["cl_ord_id"].get<std::string>();
                    if (item.contains("order_userref")) br.order_userref = item["order_userref"].get<int64_t>();
                    if (item.contains("error"))         br.error         = item["error"].get<std::string>();
                    if (item.contains("warnings"))      br.warnings      = item["warnings"].get<std::vector<std::string>>();
                    v.push_back(br);
                }
                r.orders = v;
            }
        }
        return r;
    }
};

// ── 7. BATCH CANCEL ───────────────────────────────────────────────────────────

struct BatchCancelRequest : TypedWsRequest<BatchCancelResponse> {
    std::string token;
    std::optional<std::vector<std::string>> order_ids;
    std::optional<std::vector<std::string>> cl_ord_ids;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_ids && !order_ids->empty())   params["order_id"]  = *order_ids;
        if (cl_ord_ids && !cl_ord_ids->empty()) params["cl_ord_id"] = *cl_ord_ids;

        json msg;
        msg["method"] = "batch_cancel";
        msg["params"] = params;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct BatchCancelResponse : BaseResponse {
    std::optional<int32_t> orders_cancelled;

    static BatchCancelResponse from_json(const json& j) {
        BatchCancelResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("orders_cancelled")) r.orders_cancelled = res["orders_cancelled"].get<int32_t>();
        }
        return r;
    }
};

// ── 8. EDIT ORDER ─────────────────────────────────────────────────────────────

struct EditOrderRequest : TypedWsRequest<EditOrderResponse> {
    std::string token;
    std::optional<std::string> order_id;
    std::optional<std::string> cl_ord_id;
    std::optional<double>    order_qty;
    std::optional<double>    display_qty;
    std::optional<TickPrice> limit_price;
    std::optional<Triggers>  triggers;
    std::optional<bool>      post_only;
    std::optional<std::string> deadline;
    std::optional<std::string> new_cl_ord_id;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_id)      params["order_id"]      = *order_id;
        if (cl_ord_id)     params["cl_ord_id"]      = *cl_ord_id;
        if (order_qty)     params["order_qty"]      = *order_qty;
        if (display_qty)   params["display_qty"]    = *display_qty;
        if (limit_price)   params["limit_price"]    = limit_price->to_json();
        if (triggers)      params["triggers"]       = triggers->to_json();
        if (post_only)     params["post_only"]      = *post_only;
        if (deadline)      params["deadline"]       = *deadline;
        if (new_cl_ord_id) params["new_cl_ord_id"]  = *new_cl_ord_id;

        json msg;
        msg["method"] = "edit_order";
        msg["params"] = params;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct EditOrderResponse : BaseResponse {
    std::optional<std::string> order_id;
    std::optional<std::string> original_order_id;
    std::optional<std::string> cl_ord_id;
    std::optional<std::vector<std::string>> warnings;

    static EditOrderResponse from_json(const json& j) {
        EditOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success) {
            const auto& res = j["result"];
            if (res.contains("order_id"))          r.order_id          = res["order_id"].get<std::string>();
            if (res.contains("original_order_id")) r.original_order_id = res["original_order_id"].get<std::string>();
            if (res.contains("cl_ord_id"))         r.cl_ord_id         = res["cl_ord_id"].get<std::string>();
            if (res.contains("warnings"))          r.warnings          = res["warnings"].get<std::vector<std::string>>();
        }
        return r;
    }
};

// ── 9. SUBSCRIPTIONS ─────────────────────────────────────────────────────────

enum class SubscribeChannel {
    Ticker,
    Book,
    Level3,
    OHLC,
    Trade,
    Instrument,
    Executions,
    Balances
};

inline std::string to_string(SubscribeChannel ch) {
    switch (ch) {
        case SubscribeChannel::Ticker:     return "ticker";
        case SubscribeChannel::Book:       return "book";
        case SubscribeChannel::Level3:     return "level3";
        case SubscribeChannel::OHLC:       return "ohlc";
        case SubscribeChannel::Trade:      return "trade";
        case SubscribeChannel::Instrument: return "instrument";
        case SubscribeChannel::Executions: return "executions";
        case SubscribeChannel::Balances:   return "balances";
    }
    throw std::invalid_argument("Unknown channel");
}

struct SubscribeRequest : WsRequestBase {
    SubscribeChannel channel;
    std::optional<std::vector<std::string>> symbols;
    std::optional<std::string> token;
    std::optional<int32_t>     depth;
    std::optional<int32_t>     interval;
    std::optional<bool>        snapshot;
    std::optional<bool>        snapshot_trades;

    json to_json() const {
        json params;
        params["channel"] = to_string(channel);
        if (symbols && !symbols->empty()) params["symbol"] = *symbols;
        if (token)           params["token"]           = *token;
        if (depth)           params["depth"]           = *depth;
        if (interval)        params["interval"]        = *interval;
        if (snapshot.has_value())        params["snapshot"]        = *snapshot;
        if (snapshot_trades.has_value()) params["snapshot_trades"] = *snapshot_trades;

        json msg;
        msg["method"] = "subscribe";
        msg["params"] = params;
        msg["req_id"] = req_id;
        return msg;
    }
};

struct UnsubscribeRequest {
    SubscribeChannel channel;
    std::optional<std::vector<std::string>> symbols;
    std::optional<std::string> token;
    std::optional<int64_t> req_id;

    json to_json() const {
        json params;
        params["channel"] = to_string(channel);
        if (symbols && !symbols->empty()) params["symbol"] = *symbols;
        if (token) params["token"] = *token;

        json msg;
        msg["method"] = "unsubscribe";
        msg["params"] = params;
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

struct SubscribeResponse : BaseResponse {
    std::optional<std::string> channel;
    std::optional<std::string> symbol;

    static SubscribeResponse from_json(const json& j) {
        SubscribeResponse r;
        parse_base(j, r);
        if (j.contains("result")) {
            const auto& res = j["result"];
            if (res.contains("channel")) r.channel = res["channel"].get<std::string>();
            if (res.contains("symbol"))  r.symbol  = res["symbol"].get<std::string>();
        }
        return r;
    }
};

template<typename PushMsg, SubscribeChannel Ch>
struct TypedSubscribeRequest : SubscribeRequest {
    using push_type     = PushMsg;
    using response_type = SubscribeResponse;
    static constexpr SubscribeChannel channel_value = Ch;
    TypedSubscribeRequest() { this->channel = Ch; }

    // Routing key used by ExchangeWsClient to dispatch incoming push frames.
    std::string route_key() const { return to_string(channel); }

    // Pre-built UNSUBSCRIBE frame sent by SubscriptionHandle::cancel().
    json unsubscribe_json() const {
        UnsubscribeRequest unsub;
        unsub.channel = channel;
        unsub.symbols = symbols;
        unsub.token   = token;
        return unsub.to_json();
    }
};

// ── 10. MARKET DATA - Ticker ──────────────────────────────────────────────────

struct TickerData {
    std::string symbol;
    double      bid{0.0};
    double      bid_qty{0.0};
    double      ask{0.0};
    double      ask_qty{0.0};
    double      last{0.0};
    double      volume{0.0};
    double      vwap{0.0};
    double      low{0.0};
    double      high{0.0};
    double      change{0.0};
    double      change_pct{0.0};

    static TickerData from_json(const json& j) {
        TickerData t;
        t.symbol     = j.value("symbol", "");
        t.bid        = j.value("bid", 0.0);
        t.bid_qty    = j.value("bid_qty", 0.0);
        t.ask        = j.value("ask", 0.0);
        t.ask_qty    = j.value("ask_qty", 0.0);
        t.last       = j.value("last", 0.0);
        t.volume     = j.value("volume", 0.0);
        t.vwap       = j.value("vwap", 0.0);
        t.low        = j.value("low", 0.0);
        t.high       = j.value("high", 0.0);
        t.change     = j.value("change", 0.0);
        t.change_pct = j.value("change_pct", 0.0);
        return t;
    }
};

struct TickerMessage {
    std::string channel;
    std::string type;
    std::vector<TickerData> data;

    static TickerMessage from_json(const json& j) {
        TickerMessage m;
        m.channel = j.value("channel", "");
        m.type    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data.push_back(TickerData::from_json(item));
        }
        return m;
    }
};

// ── 11. MARKET DATA - Book ────────────────────────────────────────────────────

struct BookEntry {
    double price{0.0};
    double qty{0.0};
};

struct BookData {
    std::string             symbol;
    std::vector<BookEntry>  bids;
    std::vector<BookEntry>  asks;
    std::optional<unsigned int> checksum;

    static BookData from_json(const json& j) {
        BookData b;
        b.symbol = j.value("symbol", "");
        if (j.contains("bids")) {
            for (const auto& item : j["bids"])
                b.bids.push_back({item["price"].get<double>(), item["qty"].get<double>()});
        }
        if (j.contains("asks")) {
            for (const auto& item : j["asks"])
                b.asks.push_back({item["price"].get<double>(), item["qty"].get<double>()});
        }
        if (j.contains("checksum")) b.checksum = j["checksum"].get<unsigned int>();
        return b;
    }
};

struct BookMessage {
    std::string channel;
    std::string type;
    std::vector<BookData> data;

    static BookMessage from_json(const json& j) {
        BookMessage m;
        m.channel = j.value("channel", "");
        m.type    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data.push_back(BookData::from_json(item));
        }
        return m;
    }
};

// ── 12. MARKET DATA - Trades ──────────────────────────────────────────────────

struct TradeData {
    std::string symbol;
    double      price{0.0};
    double      qty{0.0};
    std::string side;
    std::string ord_type;
    int64_t     trade_id{0};
    std::string timestamp;

    static TradeData from_json(const json& j) {
        TradeData t;
        t.symbol    = j.value("symbol", "");
        t.price     = j.value("price", 0.0);
        t.qty       = j.value("qty", 0.0);
        t.side      = j.value("side", "");
        t.ord_type  = j.value("ord_type", "");
        t.trade_id  = j.value("trade_id", int64_t{0});
        t.timestamp = j.value("timestamp", "");
        return t;
    }
};

struct TradeMessage {
    std::string channel;
    std::string type;
    std::vector<TradeData> data;

    static TradeMessage from_json(const json& j) {
        TradeMessage m;
        m.channel = j.value("channel", "");
        m.type    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data.push_back(TradeData::from_json(item));
        }
        return m;
    }
};

// ── 13. MARKET DATA - OHLC ────────────────────────────────────────────────────

struct OHLCData {
    std::string symbol;
    std::string timestamp;
    double      open{0.0};
    double      high{0.0};
    double      low{0.0};
    double      close{0.0};
    double      vwap{0.0};
    double      volume{0.0};
    int64_t     trades{0};
    std::string interval_begin;
    std::optional<int32_t> interval;

    static OHLCData from_json(const json& j) {
        OHLCData o;
        o.symbol         = j.value("symbol", "");
        o.timestamp      = j.value("timestamp", "");
        o.open           = j.value("open", 0.0);
        o.high           = j.value("high", 0.0);
        o.low            = j.value("low", 0.0);
        o.close          = j.value("close", 0.0);
        o.vwap           = j.value("vwap", 0.0);
        o.volume         = j.value("volume", 0.0);
        o.trades         = j.value("trades", int64_t{0});
        o.interval_begin = j.value("interval_begin", "");
        if (j.contains("interval")) o.interval = j["interval"].get<int32_t>();
        return o;
    }
};

struct OHLCMessage {
    std::string channel;
    std::string type;
    std::vector<OHLCData> data;

    static OHLCMessage from_json(const json& j) {
        OHLCMessage m;
        m.channel = j.value("channel", "");
        m.type    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data.push_back(OHLCData::from_json(item));
        }
        return m;
    }
};

// ── 14. MARKET DATA - Instrument ─────────────────────────────────────────────

struct AssetInfo {
    std::string id;
    std::string status;
    std::optional<int32_t> precision;
    std::optional<int32_t> precision_display;
    std::optional<bool>    borrowable;
    std::optional<double>  collateral_value;
    std::optional<double>  margin_rate;

    static AssetInfo from_json(const json& j) {
        AssetInfo a;
        a.id     = j.value("id", "");
        a.status = j.value("status", "");
        if (j.contains("precision"))         a.precision         = j["precision"].get<int32_t>();
        if (j.contains("precision_display")) a.precision_display = j["precision_display"].get<int32_t>();
        if (j.contains("borrowable"))        a.borrowable        = j["borrowable"].get<bool>();
        if (j.contains("collateral_value"))  a.collateral_value  = j["collateral_value"].get<double>();
        if (j.contains("margin_rate"))       a.margin_rate       = j["margin_rate"].get<double>();
        return a;
    }
};

struct InstrumentInfo {
    std::string symbol;
    std::string base;
    std::string quote;
    std::string status;
    double      qty_increment{0.0};
    double      qty_min{0.0};
    double      price_increment{0.0};
    double      cost_min{0.0};
    int32_t     margin_initial{0};
    std::optional<int32_t> position_limit_long;
    std::optional<int32_t> position_limit_short;
    std::optional<bool>    has_index;
    std::optional<int32_t> cost_precision;
    std::optional<int32_t> qty_precision;

    static InstrumentInfo from_json(const json& j) {
        InstrumentInfo i;
        i.symbol          = j.value("symbol", "");
        i.base            = j.value("base", "");
        i.quote           = j.value("quote", "");
        i.status          = j.value("status", "");
        i.qty_increment   = j.value("qty_increment", 0.0);
        i.qty_min         = j.value("qty_min", 0.0);
        i.price_increment = j.value("price_increment", 0.0);
        i.cost_min        = j.value("cost_min", 0.0);
        i.margin_initial  = j.value("margin_initial", 0);
        if (j.contains("position_limit_long"))  i.position_limit_long  = j["position_limit_long"].get<int32_t>();
        if (j.contains("position_limit_short")) i.position_limit_short = j["position_limit_short"].get<int32_t>();
        if (j.contains("has_index"))            i.has_index            = j["has_index"].get<bool>();
        if (j.contains("cost_precision"))       i.cost_precision       = j["cost_precision"].get<int32_t>();
        if (j.contains("qty_precision"))        i.qty_precision        = j["qty_precision"].get<int32_t>();
        return i;
    }
};

struct InstrumentMessage {
    std::string channel;
    std::string type;
    std::vector<AssetInfo>      assets;
    std::vector<InstrumentInfo> pairs;

    static InstrumentMessage from_json(const json& j) {
        InstrumentMessage m;
        m.channel = j.value("channel", "");
        m.type    = j.value("type", "");
        if (j.contains("data") && j["data"].is_object()) {
            const auto& d = j["data"];
            if (d.contains("assets") && d["assets"].is_array())
                for (const auto& item : d["assets"])
                    m.assets.push_back(AssetInfo::from_json(item));
            if (d.contains("pairs") && d["pairs"].is_array())
                for (const auto& item : d["pairs"])
                    m.pairs.push_back(InstrumentInfo::from_json(item));
        }
        return m;
    }
};

// ── 15. USER DATA - Executions ────────────────────────────────────────────────

struct ExecutionData {
    std::string exec_id;
    std::string exec_type;
    std::string order_id;
    std::string symbol;
    std::string side;
    std::string order_type;
    double      order_qty{0.0};
    double      cum_qty{0.0};
    double      leaves_qty{0.0};
    double      last_qty{0.0};
    double      last_price{0.0};
    double      avg_price{0.0};
    double      cost{0.0};
    std::string order_status;
    std::string timestamp;
    std::optional<std::string> cl_ord_id;
    std::optional<int64_t>     order_userref;
    std::optional<double>      fee;
    std::optional<std::string> fee_currency;
    std::optional<double>      limit_price;
    std::optional<std::string> time_in_force;
    std::optional<bool>        post_only;
    std::optional<bool>        margin;
    std::optional<std::string> reason;

    static ExecutionData from_json(const json& j) {
        ExecutionData e;
        e.exec_id      = j.value("exec_id", "");
        e.exec_type    = j.value("exec_type", "");
        e.order_id     = j.value("order_id", "");
        e.symbol       = j.value("symbol", "");
        e.side         = j.value("side", "");
        e.order_type   = j.value("order_type", "");
        e.order_qty    = j.value("order_qty", 0.0);
        e.cum_qty      = j.value("cum_qty", 0.0);
        e.leaves_qty   = j.value("leaves_qty", 0.0);
        e.last_qty     = j.value("last_qty", 0.0);
        e.last_price   = j.value("last_price", 0.0);
        e.avg_price    = j.value("avg_price", 0.0);
        e.cost         = j.value("cost", 0.0);
        e.order_status = j.value("order_status", "");
        e.timestamp    = j.value("timestamp", "");
        if (j.contains("cl_ord_id"))     e.cl_ord_id     = j["cl_ord_id"].get<std::string>();
        if (j.contains("order_userref")) e.order_userref = j["order_userref"].get<int64_t>();
        if (j.contains("fee"))           e.fee           = j["fee"].get<double>();
        if (j.contains("fee_currency"))  e.fee_currency  = j["fee_currency"].get<std::string>();
        if (j.contains("limit_price"))   e.limit_price   = j["limit_price"].get<double>();
        if (j.contains("time_in_force")) e.time_in_force = j["time_in_force"].get<std::string>();
        if (j.contains("post_only"))     e.post_only     = j["post_only"].get<bool>();
        if (j.contains("margin"))        e.margin        = j["margin"].get<bool>();
        if (j.contains("reason"))        e.reason        = j["reason"].get<std::string>();
        return e;
    }
};

struct ExecutionsMessage {
    std::string channel;
    std::string type;
    std::vector<ExecutionData> data;

    static ExecutionsMessage from_json(const json& j) {
        ExecutionsMessage m;
        m.channel = j.value("channel", "");
        m.type    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data.push_back(ExecutionData::from_json(item));
        }
        return m;
    }
};

// ── 16. USER DATA - Balances ──────────────────────────────────────────────────

struct BalanceData {
    std::string asset;
    double      balance{0.0};
    double      hold_trade{0.0};

    static BalanceData from_json(const json& j) {
        BalanceData b;
        b.asset      = j.value("asset", "");
        b.balance    = j.value("balance", 0.0);
        b.hold_trade = j.value("hold_trade", 0.0);
        return b;
    }
};

struct BalancesMessage {
    std::string channel;
    std::string type;
    std::vector<BalanceData> data;

    static BalancesMessage from_json(const json& j) {
        BalancesMessage m;
        m.channel = j.value("channel", "");
        m.type    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data.push_back(BalanceData::from_json(item));
        }
        return m;
    }
};

// ── 17. ADMIN - Status / Heartbeat / Ping ────────────────────────────────────

struct StatusMessage {
    std::string channel;
    std::string type;
    std::string system;
    std::string version;

    static StatusMessage from_json(const json& j) {
        StatusMessage s;
        s.channel = j.value("channel", "");
        s.type    = j.value("type", "");
        if (j.contains("data") && !j["data"].empty()) {
            const auto& d = j["data"][0];
            s.system  = d.value("system", "");
            s.version = d.value("version", "");
        }
        return s;
    }
};

struct PingRequest : TypedWsRequest<PongMessage> {
    json to_json() const {
        json msg;
        msg["method"] = "ping";
        msg["req_id"] = req_id;
        return msg;
    }
};

struct PongMessage {
    std::string method;
    std::optional<int64_t> req_id;

    static PongMessage from_json(const json& j) {
        PongMessage p;
        p.method = j.value("method", "pong");
        if (j.contains("req_id")) p.req_id = j["req_id"].get<int64_t>();
        return p;
    }
};

// ── Message Dispatcher ────────────────────────────────────────────────────────

enum class MessageKind {
    AddOrderResponse,
    AmendOrderResponse,
    CancelOrderResponse,
    CancelAllResponse,
    CancelOnDisconnectResponse,
    BatchAddResponse,
    BatchCancelResponse,
    EditOrderResponse,
    SubscribeResponse,
    UnsubscribeResponse,
    Pong,
    Ticker,
    Book,
    Level3,
    OHLC,
    Trade,
    Instrument,
    Executions,
    Balances,
    Status,
    Heartbeat,
    Unknown
};

inline MessageKind identify_message(const json& j) {
    if (j.contains("method")) {
        const auto m = j["method"].get<std::string>();
        if (m == "add_order")    return MessageKind::AddOrderResponse;
        if (m == "amend_order")  return MessageKind::AmendOrderResponse;
        if (m == "cancel_order") return MessageKind::CancelOrderResponse;
        if (m == "cancel_all")   return MessageKind::CancelAllResponse;
        if (m == "cancel_after") return MessageKind::CancelOnDisconnectResponse;
        if (m == "batch_add")    return MessageKind::BatchAddResponse;
        if (m == "batch_cancel") return MessageKind::BatchCancelResponse;
        if (m == "edit_order")   return MessageKind::EditOrderResponse;
        if (m == "subscribe")    return MessageKind::SubscribeResponse;
        if (m == "unsubscribe")  return MessageKind::UnsubscribeResponse;
        if (m == "pong")         return MessageKind::Pong;
    }
    if (j.contains("channel")) {
        const auto ch = j["channel"].get<std::string>();
        if (ch == "ticker")      return MessageKind::Ticker;
        if (ch == "book")        return MessageKind::Book;
        if (ch == "level3")      return MessageKind::Level3;
        if (ch == "ohlc")        return MessageKind::OHLC;
        if (ch == "trade")       return MessageKind::Trade;
        if (ch == "instrument")  return MessageKind::Instrument;
        if (ch == "executions")  return MessageKind::Executions;
        if (ch == "balances")    return MessageKind::Balances;
        if (ch == "status")      return MessageKind::Status;
        if (ch == "heartbeat")   return MessageKind::Heartbeat;
    }
    return MessageKind::Unknown;
}

// ── Per-channel typed subscribe request aliases ───────────────────────────────

using TickerSubscribeRequest     = TypedSubscribeRequest<TickerMessage,     SubscribeChannel::Ticker>;
using BookSubscribeRequest       = TypedSubscribeRequest<BookMessage,       SubscribeChannel::Book>;
using TradeSubscribeRequest      = TypedSubscribeRequest<TradeMessage,      SubscribeChannel::Trade>;
using OHLCSubscribeRequest       = TypedSubscribeRequest<OHLCMessage,       SubscribeChannel::OHLC>;
using InstrumentSubscribeRequest = TypedSubscribeRequest<InstrumentMessage, SubscribeChannel::Instrument>;
using ExecutionsSubscribeRequest = TypedSubscribeRequest<ExecutionsMessage,  SubscribeChannel::Executions>;
using BalancesSubscribeRequest   = TypedSubscribeRequest<BalancesMessage,   SubscribeChannel::Balances>;

// ── Kraken frame descriptor ───────────────────────────────────────────────────

inline exchange::ws::FrameDescriptor kraken_frame_descriptor(const json& j) {
    exchange::ws::FrameDescriptor desc;

    if (j.contains("req_id") && j["req_id"].is_number_integer()) {
        desc.kind           = exchange::ws::FrameKind::MethodResponse;
        desc.correlation_id = std::to_string(j["req_id"].get<int64_t>());
        return desc;
    }

    if (j.contains("channel") && j["channel"].is_string()) {
        desc.kind      = exchange::ws::FrameKind::PushMessage;
        desc.route_key = j["channel"].get<std::string>();
        return desc;
    }

    return desc;  // kind == Unknown
}

} // namespace exchange::kraken::ws
