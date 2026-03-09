// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>

#include "kraken_types.hpp"

namespace kraken::ws {

using json = nlohmann::json;

// ============================================================
// WebSocket response interfaces
//
// IWsMessage        — base for all WebSocket message types.
// IWsMethodResponse — for method-call responses (one response per request,
//                     e.g. AddOrderResponse, PongMessage).
// IWsPushMessage    — for subscription push messages (continuous stream,
//                     e.g. TickerMessage, BookMessage).
//
// All three are extension points for Step 2, where direct JSON access
// will be added.  In Step 1 they serve as pure marker interfaces.
// ============================================================

class IWsMessage : public kraken::IApiResult {
public:
    virtual ~IWsMessage() = default;
};

class IWsMethodResponse : public IWsMessage {
public:
    virtual ~IWsMethodResponse() = default;
    virtual const std::string&         method()   const = 0;
    virtual bool                       success()  const = 0;
    virtual std::optional<int64_t>     req_id()   const = 0;
    virtual std::optional<std::string> error()    const = 0;
    virtual std::optional<std::string> time_in()  const = 0;
    virtual std::optional<std::string> time_out() const = 0;
};

class IWsPushMessage : public IWsMessage {
public:
    virtual ~IWsPushMessage() = default;
};

// ============================================================
// Authentication credentials
// ============================================================

class WsCredentials {
public:
    std::string token;

    json to_json() const {
        json j;
        j["token"] = token;
        return j;
    }
};

// ============================================================
// Common sub-objects
// ============================================================

class Triggers {
public:
    double price{0.0};
    std::optional<TriggerReference> reference;   // default: last
    std::optional<PriceType>        price_type;  // default: static

    json to_json() const {
        json j;
        j["price"] = price;
        if (reference)  j["reference"]  = to_string(*reference);
        if (price_type) j["price_type"] = to_string(*price_type);
        return j;
    }

    static Triggers from_json(const json& j) {
        Triggers t;
        t.price = j.at("price").get<double>();
        if (j.contains("reference"))  t.reference  = trigger_ref_from_string(j["reference"].get<std::string>());
        if (j.contains("price_type")) t.price_type = price_type_from_string(j["price_type"].get<std::string>());
        return t;
    }
};

class Conditional {
public:
    std::optional<OrderType> order_type;
    std::optional<double>    limit_price;
    std::optional<PriceType> limit_price_type;
    std::optional<double>    trigger_price;
    std::optional<PriceType> trigger_price_type;

    json to_json() const {
        json j;
        if (order_type)         j["order_type"]         = to_string(*order_type);
        if (limit_price)        j["limit_price"]        = *limit_price;
        if (limit_price_type)   j["limit_price_type"]   = to_string(*limit_price_type);
        if (trigger_price)      j["trigger_price"]      = *trigger_price;
        if (trigger_price_type) j["trigger_price_type"] = to_string(*trigger_price_type);
        return j;
    }

    static Conditional from_json(const json& j) {
        Conditional c;
        if (j.contains("order_type"))         c.order_type         = order_type_from_string(j["order_type"].get<std::string>());
        if (j.contains("limit_price"))        c.limit_price        = j["limit_price"].get<double>();
        if (j.contains("limit_price_type"))   c.limit_price_type   = price_type_from_string(j["limit_price_type"].get<std::string>());
        if (j.contains("trigger_price"))      c.trigger_price      = j["trigger_price"].get<double>();
        if (j.contains("trigger_price_type")) c.trigger_price_type = price_type_from_string(j["trigger_price_type"].get<std::string>());
        return c;
    }
};

// ============================================================
// Typed request bases  (mirror TypedPublicRequest/TypedPrivateRequest from REST)
// ============================================================

// Method-call requests: declares the expected single response type.
template<typename R>
class TypedWsRequest {
public:
    using response_type = R;
};

// Forward declarations of all response types so that TypedWsRequest<Resp>
// can be used as a base class before the response class is fully defined.
// (TypedWsRequest<R> only stores 'using response_type = R', so R need not
// be complete – but the name must be visible in scope.)
class AddOrderResponse;
class AmendOrderResponse;
class CancelOrderResponse;
class CancelAllResponse;
class CancelOnDisconnectResponse;
class BatchAddResponse;
class BatchCancelResponse;
class EditOrderResponse;
class PongMessage;

// ============================================================
// Response base
// ============================================================

class BaseResponse : public IWsMethodResponse {
private:
    std::string              method_;
    bool                     success_{false};
    std::optional<int64_t>   req_id_;
    std::optional<std::string> error_;
    std::optional<std::string> time_in_;
    std::optional<std::string> time_out_;
public:
    const std::string&         method()   const override { return method_; }
    bool                       success()  const override { return success_; }
    std::optional<int64_t>     req_id()   const override { return req_id_; }
    std::optional<std::string> error()    const override { return error_; }
    std::optional<std::string> time_in()  const override { return time_in_; }
    std::optional<std::string> time_out() const override { return time_out_; }

    static void parse_base(const json& j, BaseResponse& r) {
        if (j.contains("method"))   r.method_   = j["method"].get<std::string>();
        if (j.contains("success"))  r.success_  = j["success"].get<bool>();
        if (j.contains("req_id"))   r.req_id_   = j["req_id"].get<int64_t>();
        if (j.contains("error"))    r.error_    = j["error"].get<std::string>();
        if (j.contains("time_in"))  r.time_in_  = j["time_in"].get<std::string>();
        if (j.contains("time_out")) r.time_out_ = j["time_out"].get<std::string>();
    }
};

// ============================================================
//  1. ADD ORDER
// ============================================================

class AddOrderRequest : public TypedWsRequest<AddOrderResponse> {
public:
    // Required
    OrderType   order_type;
    Side        side;
    double      order_qty{0.0};
    std::string symbol;
    std::string token;

    // Optional pricing
    std::optional<double>    limit_price;
    std::optional<PriceType> limit_price_type;

    // Optional trigger section (for triggered order types)
    std::optional<Triggers>   triggers;

    // Optional OTO conditional close order
    std::optional<Conditional> conditional;

    // Optional flags
    std::optional<TimeInForce>   time_in_force;
    std::optional<bool>          margin;
    std::optional<bool>          post_only;
    std::optional<bool>          reduce_only;
    std::optional<std::string>   effective_time;  // RFC3339
    std::optional<std::string>   expire_time;     // RFC3339 (GTD only)
    std::optional<std::string>   deadline;        // RFC3339
    std::optional<std::string>   cl_ord_id;
    std::optional<int64_t>       order_userref;
    std::optional<double>        display_qty;     // iceberg only
    std::optional<FeePreference> fee_preference;
    std::optional<StpType>       stp_type;
    std::optional<double>        cash_order_qty;  // buy market without margin
    std::optional<bool>          validate;
    std::optional<std::string>   sender_sub_id;
    std::optional<int64_t>       req_id;

    json to_json() const {
        json params;
        params["order_type"] = to_string(order_type);
        params["side"]       = to_string(side);
        params["order_qty"]  = order_qty;
        params["symbol"]     = symbol;
        params["token"]      = token;

        if (limit_price)      params["limit_price"]      = *limit_price;
        if (limit_price_type) params["limit_price_type"] = to_string(*limit_price_type);
        if (triggers)         params["triggers"]         = triggers->to_json();
        if (conditional)      params["conditional"]      = conditional->to_json();
        if (time_in_force)    params["time_in_force"]    = to_string(*time_in_force);
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
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class AddOrderResponse : public BaseResponse {
private:
    std::optional<std::string> order_id_;
    std::optional<std::string> cl_ord_id_;
    std::optional<int64_t>     order_userref_;
    std::optional<std::vector<std::string>> warnings_;
public:
    std::optional<std::string> order_id()      const { return order_id_; }
    std::optional<std::string> cl_ord_id()     const { return cl_ord_id_; }
    std::optional<int64_t>     order_userref() const { return order_userref_; }
    std::optional<std::vector<std::string>> warnings() const { return warnings_; }

    static AddOrderResponse from_json(const json& j) {
        AddOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("order_id"))      r.order_id_      = res["order_id"].get<std::string>();
            if (res.contains("cl_ord_id"))     r.cl_ord_id_     = res["cl_ord_id"].get<std::string>();
            if (res.contains("order_userref")) r.order_userref_ = res["order_userref"].get<int64_t>();
            if (res.contains("warnings"))      r.warnings_      = res["warnings"].get<std::vector<std::string>>();
        }
        return r;
    }
};

// ============================================================
//  2. AMEND ORDER
// ============================================================

class AmendOrderRequest : public TypedWsRequest<AmendOrderResponse> {
public:
    std::string token;
    // Must provide one of:
    std::optional<std::string> order_id;
    std::optional<std::string> cl_ord_id;

    std::optional<double>    order_qty;
    std::optional<double>    display_qty;
    std::optional<double>    limit_price;
    std::optional<PriceType> limit_price_type;
    std::optional<Triggers>  triggers;
    std::optional<double>    post_only_price; // amend to post-only at this price
    std::optional<std::string> deadline;
    std::optional<int64_t>   req_id;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_id)         params["order_id"]         = *order_id;
        if (cl_ord_id)        params["cl_ord_id"]        = *cl_ord_id;
        if (order_qty)        params["order_qty"]        = *order_qty;
        if (display_qty)      params["display_qty"]      = *display_qty;
        if (limit_price)      params["limit_price"]      = *limit_price;
        if (limit_price_type) params["limit_price_type"] = to_string(*limit_price_type);
        if (triggers)         params["triggers"]         = triggers->to_json();
        if (post_only_price)  params["post_only_price"]  = *post_only_price;
        if (deadline)         params["deadline"]         = *deadline;

        json msg;
        msg["method"] = "amend_order";
        msg["params"] = params;
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class AmendOrderResponse : public BaseResponse {
private:
    std::optional<std::string> order_id_;
    std::optional<std::string> cl_ord_id_;
    std::optional<std::vector<std::string>> warnings_;
public:
    std::optional<std::string> order_id()  const { return order_id_; }
    std::optional<std::string> cl_ord_id() const { return cl_ord_id_; }
    std::optional<std::vector<std::string>> warnings() const { return warnings_; }

    static AmendOrderResponse from_json(const json& j) {
        AmendOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("order_id"))  r.order_id_  = res["order_id"].get<std::string>();
            if (res.contains("cl_ord_id")) r.cl_ord_id_ = res["cl_ord_id"].get<std::string>();
            if (res.contains("warnings"))  r.warnings_  = res["warnings"].get<std::vector<std::string>>();
        }
        return r;
    }
};

// ============================================================
//  3. CANCEL ORDER
// ============================================================

class CancelOrderRequest : public TypedWsRequest<CancelOrderResponse> {
public:
    std::string token;
    // Provide one or more order ids OR cl_ord_ids
    std::optional<std::vector<std::string>> order_ids;
    std::optional<std::vector<std::string>> cl_ord_ids;
    std::optional<int64_t> req_id;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_ids && !order_ids->empty())   params["order_ids"]  = *order_ids;
        if (cl_ord_ids && !cl_ord_ids->empty()) params["cl_ord_ids"] = *cl_ord_ids;

        json msg;
        msg["method"] = "cancel_order";
        msg["params"] = params;
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class CancelOrderResult {
private:
    std::string order_id_;
    bool        success_{false};
    std::optional<std::string> error_;
public:
    const std::string&         order_id() const { return order_id_; }
    bool                       success()  const { return success_; }
    std::optional<std::string> error()    const { return error_; }

    static CancelOrderResult from_json(const json& item) {
        CancelOrderResult cr;
        cr.order_id_ = item.at("order_id").get<std::string>();
        cr.success_  = item.value("success", false);
        if (item.contains("error")) cr.error_ = item["error"].get<std::string>();
        return cr;
    }
};

class CancelOrderResponse : public BaseResponse {
private:
    std::optional<std::vector<CancelOrderResult>> orders_cancelled_;
public:
    std::optional<std::vector<CancelOrderResult>> orders_cancelled() const { return orders_cancelled_; }

    static CancelOrderResponse from_json(const json& j) {
        CancelOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("orders_cancelled")) {
                std::vector<CancelOrderResult> v;
                for (const auto& item : res["orders_cancelled"])
                    v.push_back(CancelOrderResult::from_json(item));
                r.orders_cancelled_ = v;
            }
        }
        return r;
    }
};

// ============================================================
//  4. CANCEL ALL
// ============================================================

class CancelAllRequest : public TypedWsRequest<CancelAllResponse> {
public:
    std::string token;
    std::optional<int64_t> req_id;

    json to_json() const {
        json msg;
        msg["method"] = "cancel_all";
        msg["params"]["token"] = token;
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class CancelAllResponse : public BaseResponse {
private:
    std::optional<int32_t> count_;  // number of orders cancelled
public:
    std::optional<int32_t> count() const { return count_; }

    static CancelAllResponse from_json(const json& j) {
        CancelAllResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("count")) r.count_ = res["count"].get<int32_t>();
        }
        return r;
    }
};

// ============================================================
//  5. CANCEL ON DISCONNECT (cancel_after)
// ============================================================

class CancelOnDisconnectRequest : public TypedWsRequest<CancelOnDisconnectResponse> {
public:
    std::string token;
    int32_t     timeout{60};  // seconds; 0 = disable
    std::optional<int64_t> req_id;

    json to_json() const {
        json msg;
        msg["method"] = "cancel_after";
        msg["params"]["token"]   = token;
        msg["params"]["timeout"] = timeout;
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class CancelOnDisconnectResponse : public BaseResponse {
private:
    std::optional<std::string> current_time_;
    std::optional<std::string> trigger_time_;
public:
    std::optional<std::string> current_time() const { return current_time_; }
    std::optional<std::string> trigger_time() const { return trigger_time_; }

    static CancelOnDisconnectResponse from_json(const json& j) {
        CancelOnDisconnectResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("current_time")) r.current_time_ = res["current_time"].get<std::string>();
            if (res.contains("trigger_time")) r.trigger_time_ = res["trigger_time"].get<std::string>();
        }
        return r;
    }
};

// ============================================================
//  6. BATCH ADD
// ============================================================

class BatchAddRequest : public TypedWsRequest<BatchAddResponse> {
public:
    std::string token;
    std::string symbol;
    std::optional<std::string> deadline;
    std::optional<bool>        validate;
    std::optional<int64_t>     req_id;

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
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class BatchAddResult {
private:
    std::string              order_id_;
    bool                     success_{false};
    std::optional<std::string> cl_ord_id_;
    std::optional<int64_t>   order_userref_;
    std::optional<std::string> error_;
    std::optional<std::vector<std::string>> warnings_;
public:
    const std::string&         order_id()      const { return order_id_; }
    bool                       success()       const { return success_; }
    std::optional<std::string> cl_ord_id()     const { return cl_ord_id_; }
    std::optional<int64_t>     order_userref() const { return order_userref_; }
    std::optional<std::string> error()         const { return error_; }
    std::optional<std::vector<std::string>> warnings() const { return warnings_; }

    static BatchAddResult from_json(const json& item) {
        BatchAddResult br;
        br.success_  = item.value("success", false);
        if (item.contains("order_id"))      br.order_id_      = item["order_id"].get<std::string>();
        if (item.contains("cl_ord_id"))     br.cl_ord_id_     = item["cl_ord_id"].get<std::string>();
        if (item.contains("order_userref")) br.order_userref_ = item["order_userref"].get<int64_t>();
        if (item.contains("error"))         br.error_         = item["error"].get<std::string>();
        if (item.contains("warnings"))      br.warnings_      = item["warnings"].get<std::vector<std::string>>();
        return br;
    }
};

class BatchAddResponse : public BaseResponse {
private:
    std::optional<std::vector<BatchAddResult>> orders_;
public:
    std::optional<std::vector<BatchAddResult>> orders() const { return orders_; }

    static BatchAddResponse from_json(const json& j) {
        BatchAddResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("orders")) {
                std::vector<BatchAddResult> v;
                for (const auto& item : res["orders"])
                    v.push_back(BatchAddResult::from_json(item));
                r.orders_ = v;
            }
        }
        return r;
    }
};

// ============================================================
//  7. BATCH CANCEL
// ============================================================

class BatchCancelRequest : public TypedWsRequest<BatchCancelResponse> {
public:
    std::string token;
    std::optional<std::vector<std::string>> order_ids;
    std::optional<std::vector<std::string>> cl_ord_ids;
    std::optional<int64_t> req_id;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_ids && !order_ids->empty())   params["order_ids"]  = *order_ids;
        if (cl_ord_ids && !cl_ord_ids->empty()) params["cl_ord_ids"] = *cl_ord_ids;

        json msg;
        msg["method"] = "batch_cancel";
        msg["params"] = params;
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class BatchCancelResponse : public BaseResponse {
private:
    std::optional<int32_t> orders_cancelled_;
public:
    std::optional<int32_t> orders_cancelled() const { return orders_cancelled_; }

    static BatchCancelResponse from_json(const json& j) {
        BatchCancelResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("orders_cancelled")) r.orders_cancelled_ = res["orders_cancelled"].get<int32_t>();
        }
        return r;
    }
};

// ============================================================
//  8. EDIT ORDER
// ============================================================

class EditOrderRequest : public TypedWsRequest<EditOrderResponse> {
public:
    std::string token;
    // Must provide one of:
    std::optional<std::string> order_id;
    std::optional<std::string> cl_ord_id;

    std::optional<double>    order_qty;
    std::optional<double>    display_qty;
    std::optional<double>    limit_price;
    std::optional<Triggers>  triggers;
    std::optional<bool>      post_only;
    std::optional<std::string> deadline;
    std::optional<std::string> new_cl_ord_id;
    std::optional<int64_t>   req_id;

    json to_json() const {
        json params;
        params["token"] = token;
        if (order_id)      params["order_id"]      = *order_id;
        if (cl_ord_id)     params["cl_ord_id"]      = *cl_ord_id;
        if (order_qty)     params["order_qty"]      = *order_qty;
        if (display_qty)   params["display_qty"]    = *display_qty;
        if (limit_price)   params["limit_price"]    = *limit_price;
        if (triggers)      params["triggers"]       = triggers->to_json();
        if (post_only)     params["post_only"]      = *post_only;
        if (deadline)      params["deadline"]       = *deadline;
        if (new_cl_ord_id) params["new_cl_ord_id"]  = *new_cl_ord_id;

        json msg;
        msg["method"] = "edit_order";
        msg["params"] = params;
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class EditOrderResponse : public BaseResponse {
private:
    std::optional<std::string> order_id_;
    std::optional<std::string> original_order_id_;
    std::optional<std::string> cl_ord_id_;
    std::optional<std::vector<std::string>> warnings_;
public:
    std::optional<std::string> order_id()          const { return order_id_; }
    std::optional<std::string> original_order_id() const { return original_order_id_; }
    std::optional<std::string> cl_ord_id()         const { return cl_ord_id_; }
    std::optional<std::vector<std::string>> warnings() const { return warnings_; }

    static EditOrderResponse from_json(const json& j) {
        EditOrderResponse r;
        parse_base(j, r);
        if (j.contains("result") && r.success()) {
            const auto& res = j["result"];
            if (res.contains("order_id"))          r.order_id_          = res["order_id"].get<std::string>();
            if (res.contains("original_order_id")) r.original_order_id_ = res["original_order_id"].get<std::string>();
            if (res.contains("cl_ord_id"))         r.cl_ord_id_         = res["cl_ord_id"].get<std::string>();
            if (res.contains("warnings"))          r.warnings_          = res["warnings"].get<std::vector<std::string>>();
        }
        return r;
    }
};

// ============================================================
//  9. MARKET DATA SUBSCRIPTIONS
// ============================================================

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

class SubscribeRequest {
public:
    SubscribeChannel channel;
    std::optional<std::vector<std::string>> symbols;  // for market data channels
    std::optional<std::string> token;   // required for authenticated channels
    std::optional<int32_t>     depth;   // for book channel (10, 25, 100, 500, 1000)
    std::optional<int32_t>     interval; // for OHLC (minutes: 1,5,15,30,60,240,1440,10080,21600)
    std::optional<bool>        snapshot; // whether to send snapshot on subscribe
    std::optional<bool>        snapshot_trades; // executions channel
    std::optional<int64_t>     req_id;

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
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class UnsubscribeRequest {
public:
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

class SubscribeResponse : public BaseResponse {
private:
    std::optional<std::string> channel_;
    std::optional<std::string> symbol_;
public:
    std::optional<std::string> channel() const { return channel_; }
    std::optional<std::string> symbol()  const { return symbol_; }

    static SubscribeResponse from_json(const json& j) {
        SubscribeResponse r;
        parse_base(j, r);
        if (j.contains("result")) {
            const auto& res = j["result"];
            if (res.contains("channel")) r.channel_ = res["channel"].get<std::string>();
            if (res.contains("symbol"))  r.symbol_  = res["symbol"].get<std::string>();
        }
        return r;
    }
};

// ============================================================
// Typed subscription request base
//
// Inherits all SubscribeRequest fields (channel, symbols, token, depth, …)
// and adds compile-time type information:
//   push_type     — the push-data message type streamed after a successful ack
//   response_type — SubscribeResponse (the Phase 3 acknowledgement)
//
// Per-channel convenience aliases are provided at the bottom of this file.
// ============================================================

template<typename PushMsg, SubscribeChannel Ch>
class TypedSubscribeRequest : public SubscribeRequest {
public:
    using push_type     = PushMsg;
    using response_type = SubscribeResponse;
    static constexpr SubscribeChannel channel_value = Ch;
    TypedSubscribeRequest() { this->channel = Ch; }
};

// ============================================================
//  10. MARKET DATA - Ticker (Level 1)
// ============================================================

class TickerData {
private:
    std::string symbol_;
    double      bid_{0.0};
    double      bid_qty_{0.0};
    double      ask_{0.0};
    double      ask_qty_{0.0};
    double      last_{0.0};
    double      volume_{0.0};
    double      vwap_{0.0};
    double      low_{0.0};
    double      high_{0.0};
    double      change_{0.0};
    double      change_pct_{0.0};
public:
    const std::string& symbol()     const { return symbol_; }
    double             bid()        const { return bid_; }
    double             bid_qty()    const { return bid_qty_; }
    double             ask()        const { return ask_; }
    double             ask_qty()    const { return ask_qty_; }
    double             last()       const { return last_; }
    double             volume()     const { return volume_; }
    double             vwap()       const { return vwap_; }
    double             low()        const { return low_; }
    double             high()       const { return high_; }
    double             change()     const { return change_; }
    double             change_pct() const { return change_pct_; }

    static TickerData from_json(const json& j) {
        TickerData t;
        t.symbol_     = j.value("symbol", "");
        t.bid_        = j.value("bid", 0.0);
        t.bid_qty_    = j.value("bid_qty", 0.0);
        t.ask_        = j.value("ask", 0.0);
        t.ask_qty_    = j.value("ask_qty", 0.0);
        t.last_       = j.value("last", 0.0);
        t.volume_     = j.value("volume", 0.0);
        t.vwap_       = j.value("vwap", 0.0);
        t.low_        = j.value("low", 0.0);
        t.high_       = j.value("high", 0.0);
        t.change_     = j.value("change", 0.0);
        t.change_pct_ = j.value("change_pct", 0.0);
        return t;
    }
};

class TickerMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;   // "snapshot" | "update"
    std::vector<TickerData> data_;
public:
    const std::string&              channel() const { return channel_; }
    const std::string&              type()    const { return type_; }
    const std::vector<TickerData>&  data()    const { return data_; }

    static TickerMessage from_json(const json& j) {
        TickerMessage m;
        m.channel_ = j.value("channel", "");
        m.type_    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data_.push_back(TickerData::from_json(item));
        }
        return m;
    }
};

// ============================================================
//  11. MARKET DATA - Book (Level 2)
// ============================================================

class BookEntry {
private:
    double price_{0.0};
    double qty_{0.0};
public:
    double price() const { return price_; }
    double qty()   const { return qty_; }

    static BookEntry from_json(const json& item) {
        BookEntry e;
        e.price_ = item[0].get<double>();
        e.qty_   = item[1].get<double>();
        return e;
    }
};

class BookData {
private:
    std::string             symbol_;
    std::vector<BookEntry>  bids_;
    std::vector<BookEntry>  asks_;
    std::optional<std::string> checksum_;
public:
    const std::string&             symbol()   const { return symbol_; }
    const std::vector<BookEntry>&  bids()     const { return bids_; }
    const std::vector<BookEntry>&  asks()     const { return asks_; }
    std::optional<std::string>     checksum() const { return checksum_; }

    static BookData from_json(const json& j) {
        BookData b;
        b.symbol_ = j.value("symbol", "");
        if (j.contains("bids")) {
            for (const auto& item : j["bids"])
                b.bids_.push_back(BookEntry::from_json(item));
        }
        if (j.contains("asks")) {
            for (const auto& item : j["asks"])
                b.asks_.push_back(BookEntry::from_json(item));
        }
        if (j.contains("checksum")) b.checksum_ = j["checksum"].get<std::string>();
        return b;
    }
};

class BookMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;   // "snapshot" | "update"
    std::vector<BookData> data_;
public:
    const std::string&             channel() const { return channel_; }
    const std::string&             type()    const { return type_; }
    const std::vector<BookData>&   data()    const { return data_; }

    static BookMessage from_json(const json& j) {
        BookMessage m;
        m.channel_ = j.value("channel", "");
        m.type_    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data_.push_back(BookData::from_json(item));
        }
        return m;
    }
};

// ============================================================
//  12. MARKET DATA - Trades
// ============================================================

class TradeData {
private:
    std::string symbol_;
    double      price_{0.0};
    double      qty_{0.0};
    std::string side_;        // "buy" | "sell"
    std::string ord_type_;    // "limit" | "market"
    std::string trade_id_;
    std::string timestamp_;
public:
    const std::string& symbol()    const { return symbol_; }
    double             price()     const { return price_; }
    double             qty()       const { return qty_; }
    const std::string& side()      const { return side_; }
    const std::string& ord_type()  const { return ord_type_; }
    const std::string& trade_id()  const { return trade_id_; }
    const std::string& timestamp() const { return timestamp_; }

    static TradeData from_json(const json& j) {
        TradeData t;
        t.symbol_    = j.value("symbol", "");
        t.price_     = j.value("price", 0.0);
        t.qty_       = j.value("qty", 0.0);
        t.side_      = j.value("side", "");
        t.ord_type_  = j.value("ord_type", "");
        t.trade_id_  = j.value("trade_id", "");
        t.timestamp_ = j.value("timestamp", "");
        return t;
    }
};

class TradeMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;
    std::vector<TradeData> data_;
public:
    const std::string&              channel() const { return channel_; }
    const std::string&              type()    const { return type_; }
    const std::vector<TradeData>&   data()    const { return data_; }

    static TradeMessage from_json(const json& j) {
        TradeMessage m;
        m.channel_ = j.value("channel", "");
        m.type_    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data_.push_back(TradeData::from_json(item));
        }
        return m;
    }
};

// ============================================================
//  13. MARKET DATA - OHLC / Candles
// ============================================================

class OHLCData {
private:
    std::string symbol_;
    std::string timestamp_;  // candle open time
    double      open_{0.0};
    double      high_{0.0};
    double      low_{0.0};
    double      close_{0.0};
    double      vwap_{0.0};
    double      volume_{0.0};
    int64_t     trades_{0};
    int32_t     interval_begin_{0};
public:
    const std::string& symbol()         const { return symbol_; }
    const std::string& timestamp()      const { return timestamp_; }
    double             open()           const { return open_; }
    double             high()           const { return high_; }
    double             low()            const { return low_; }
    double             close()          const { return close_; }
    double             vwap()           const { return vwap_; }
    double             volume()         const { return volume_; }
    int64_t            trades()         const { return trades_; }
    int32_t            interval_begin() const { return interval_begin_; }

    static OHLCData from_json(const json& j) {
        OHLCData o;
        o.symbol_         = j.value("symbol", "");
        o.timestamp_      = j.value("timestamp", "");
        o.open_           = j.value("open", 0.0);
        o.high_           = j.value("high", 0.0);
        o.low_            = j.value("low", 0.0);
        o.close_          = j.value("close", 0.0);
        o.vwap_           = j.value("vwap", 0.0);
        o.volume_         = j.value("volume", 0.0);
        o.trades_         = j.value("trades", int64_t{0});
        o.interval_begin_ = j.value("interval_begin", 0);
        return o;
    }
};

class OHLCMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;
    std::vector<OHLCData> data_;
public:
    const std::string&             channel() const { return channel_; }
    const std::string&             type()    const { return type_; }
    const std::vector<OHLCData>&   data()    const { return data_; }

    static OHLCMessage from_json(const json& j) {
        OHLCMessage m;
        m.channel_ = j.value("channel", "");
        m.type_    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data_.push_back(OHLCData::from_json(item));
        }
        return m;
    }
};

// ============================================================
//  14. MARKET DATA - Instrument
// ============================================================

class InstrumentInfo {
private:
    std::string symbol_;
    std::string base_;
    std::string quote_;
    std::string status_;
    double      qty_increment_{0.0};
    double      qty_min_{0.0};
    double      price_increment_{0.0};
    double      cost_min_{0.0};
    int32_t     margin_initial_{0};
    std::optional<int32_t> position_limit_long_;
    std::optional<int32_t> position_limit_short_;
public:
    const std::string& symbol()          const { return symbol_; }
    const std::string& base()            const { return base_; }
    const std::string& quote()           const { return quote_; }
    const std::string& status()          const { return status_; }
    double             qty_increment()   const { return qty_increment_; }
    double             qty_min()         const { return qty_min_; }
    double             price_increment() const { return price_increment_; }
    double             cost_min()        const { return cost_min_; }
    int32_t            margin_initial()  const { return margin_initial_; }
    std::optional<int32_t> position_limit_long()  const { return position_limit_long_; }
    std::optional<int32_t> position_limit_short() const { return position_limit_short_; }

    static InstrumentInfo from_json(const json& j) {
        InstrumentInfo i;
        i.symbol_          = j.value("symbol", "");
        i.base_            = j.value("base", "");
        i.quote_           = j.value("quote", "");
        i.status_          = j.value("status", "");
        i.qty_increment_   = j.value("qty_increment", 0.0);
        i.qty_min_         = j.value("qty_min", 0.0);
        i.price_increment_ = j.value("price_increment", 0.0);
        i.cost_min_        = j.value("cost_min", 0.0);
        i.margin_initial_  = j.value("margin_initial", 0);
        if (j.contains("position_limit_long"))  i.position_limit_long_  = j["position_limit_long"].get<int32_t>();
        if (j.contains("position_limit_short")) i.position_limit_short_ = j["position_limit_short"].get<int32_t>();
        return i;
    }
};

class InstrumentMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;
    std::vector<InstrumentInfo> data_;
public:
    const std::string&                  channel() const { return channel_; }
    const std::string&                  type()    const { return type_; }
    const std::vector<InstrumentInfo>&  data()    const { return data_; }

    static InstrumentMessage from_json(const json& j) {
        InstrumentMessage m;
        m.channel_ = j.value("channel", "");
        m.type_    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data_.push_back(InstrumentInfo::from_json(item));
        }
        return m;
    }
};

// ============================================================
//  15. USER DATA - Executions
// ============================================================

class ExecutionData {
private:
    std::string exec_id_;
    std::string exec_type_;        // "filled", "canceled", "pending_new", etc.
    std::string order_id_;
    std::string symbol_;
    std::string side_;
    std::string order_type_;
    double      order_qty_{0.0};
    double      cum_qty_{0.0};
    double      leaves_qty_{0.0};
    double      last_qty_{0.0};
    double      last_price_{0.0};
    double      avg_price_{0.0};
    double      cost_{0.0};
    std::string order_status_;
    std::string timestamp_;
    std::optional<std::string> cl_ord_id_;
    std::optional<int64_t>     order_userref_;
    std::optional<double>      fee_;
    std::optional<std::string> fee_currency_;
    std::optional<double>      limit_price_;
    std::optional<std::string> time_in_force_;
    std::optional<bool>        post_only_;
    std::optional<bool>        margin_;
    std::optional<std::string> reason_;  // cancel reason
public:
    const std::string& exec_id()      const { return exec_id_; }
    const std::string& exec_type()    const { return exec_type_; }
    const std::string& order_id()     const { return order_id_; }
    const std::string& symbol()       const { return symbol_; }
    const std::string& side()         const { return side_; }
    const std::string& order_type()   const { return order_type_; }
    double             order_qty()    const { return order_qty_; }
    double             cum_qty()      const { return cum_qty_; }
    double             leaves_qty()   const { return leaves_qty_; }
    double             last_qty()     const { return last_qty_; }
    double             last_price()   const { return last_price_; }
    double             avg_price()    const { return avg_price_; }
    double             cost()         const { return cost_; }
    const std::string& order_status() const { return order_status_; }
    const std::string& timestamp()    const { return timestamp_; }
    std::optional<std::string> cl_ord_id()     const { return cl_ord_id_; }
    std::optional<int64_t>     order_userref() const { return order_userref_; }
    std::optional<double>      fee()           const { return fee_; }
    std::optional<std::string> fee_currency()  const { return fee_currency_; }
    std::optional<double>      limit_price()   const { return limit_price_; }
    std::optional<std::string> time_in_force() const { return time_in_force_; }
    std::optional<bool>        post_only()     const { return post_only_; }
    std::optional<bool>        margin()        const { return margin_; }
    std::optional<std::string> reason()        const { return reason_; }

    static ExecutionData from_json(const json& j) {
        ExecutionData e;
        e.exec_id_      = j.value("exec_id", "");
        e.exec_type_    = j.value("exec_type", "");
        e.order_id_     = j.value("order_id", "");
        e.symbol_       = j.value("symbol", "");
        e.side_         = j.value("side", "");
        e.order_type_   = j.value("order_type", "");
        e.order_qty_    = j.value("order_qty", 0.0);
        e.cum_qty_      = j.value("cum_qty", 0.0);
        e.leaves_qty_   = j.value("leaves_qty", 0.0);
        e.last_qty_     = j.value("last_qty", 0.0);
        e.last_price_   = j.value("last_price", 0.0);
        e.avg_price_    = j.value("avg_price", 0.0);
        e.cost_         = j.value("cost", 0.0);
        e.order_status_ = j.value("order_status", "");
        e.timestamp_    = j.value("timestamp", "");
        if (j.contains("cl_ord_id"))     e.cl_ord_id_     = j["cl_ord_id"].get<std::string>();
        if (j.contains("order_userref")) e.order_userref_ = j["order_userref"].get<int64_t>();
        if (j.contains("fee"))           e.fee_           = j["fee"].get<double>();
        if (j.contains("fee_currency"))  e.fee_currency_  = j["fee_currency"].get<std::string>();
        if (j.contains("limit_price"))   e.limit_price_   = j["limit_price"].get<double>();
        if (j.contains("time_in_force")) e.time_in_force_ = j["time_in_force"].get<std::string>();
        if (j.contains("post_only"))     e.post_only_     = j["post_only"].get<bool>();
        if (j.contains("margin"))        e.margin_        = j["margin"].get<bool>();
        if (j.contains("reason"))        e.reason_        = j["reason"].get<std::string>();
        return e;
    }
};

class ExecutionsMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;
    std::vector<ExecutionData> data_;
public:
    const std::string&                  channel() const { return channel_; }
    const std::string&                  type()    const { return type_; }
    const std::vector<ExecutionData>&   data()    const { return data_; }

    static ExecutionsMessage from_json(const json& j) {
        ExecutionsMessage m;
        m.channel_ = j.value("channel", "");
        m.type_    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data_.push_back(ExecutionData::from_json(item));
        }
        return m;
    }
};

// ============================================================
//  16. USER DATA - Balances
// ============================================================

class BalanceData {
private:
    std::string asset_;
    double      balance_{0.0};
    double      hold_trade_{0.0};
public:
    const std::string& asset()      const { return asset_; }
    double             balance()    const { return balance_; }
    double             hold_trade() const { return hold_trade_; }

    static BalanceData from_json(const json& j) {
        BalanceData b;
        b.asset_       = j.value("asset", "");
        b.balance_     = j.value("balance", 0.0);
        b.hold_trade_  = j.value("hold_trade", 0.0);
        return b;
    }
};

class BalancesMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;
    std::vector<BalanceData> data_;
public:
    const std::string&               channel() const { return channel_; }
    const std::string&               type()    const { return type_; }
    const std::vector<BalanceData>&  data()    const { return data_; }

    static BalancesMessage from_json(const json& j) {
        BalancesMessage m;
        m.channel_ = j.value("channel", "");
        m.type_    = j.value("type", "");
        if (j.contains("data")) {
            for (const auto& item : j["data"])
                m.data_.push_back(BalanceData::from_json(item));
        }
        return m;
    }
};

// ============================================================
//  17. ADMIN - Status / Heartbeat / Ping
// ============================================================

class StatusMessage : public IWsPushMessage {
private:
    std::string channel_;
    std::string type_;
    std::string system_;    // "online" | "maintenance"
    std::string version_;
public:
    const std::string& channel() const { return channel_; }
    const std::string& type()    const { return type_; }
    const std::string& system()  const { return system_; }
    const std::string& version() const { return version_; }

    static StatusMessage from_json(const json& j) {
        StatusMessage s;
        s.channel_ = j.value("channel", "");
        s.type_    = j.value("type", "");
        if (j.contains("data") && !j["data"].empty()) {
            const auto& d = j["data"][0];
            s.system_  = d.value("system", "");
            s.version_ = d.value("version", "");
        }
        return s;
    }
};

class PingRequest : public TypedWsRequest<PongMessage> {
public:
    std::optional<int64_t> req_id;

    json to_json() const {
        json msg;
        msg["method"] = "ping";
        if (req_id) msg["req_id"] = *req_id;
        return msg;
    }
};

class PongMessage : public IWsMethodResponse {
private:
    std::string            method_{"pong"};
    bool                   success_{true};
    std::optional<int64_t> req_id_;
public:
    const std::string&         method()   const override { return method_; }
    bool                       success()  const override { return success_; }
    std::optional<int64_t>     req_id()   const override { return req_id_; }
    std::optional<std::string> error()    const override { return std::nullopt; }
    std::optional<std::string> time_in()  const override { return std::nullopt; }
    std::optional<std::string> time_out() const override { return std::nullopt; }

    static PongMessage from_json(const json& j) {
        PongMessage p;
        p.method_ = j.value("method", "pong");
        if (j.contains("req_id")) p.req_id_ = j["req_id"].get<int64_t>();
        return p;
    }
};

// ============================================================
//  Message Dispatcher
//  Identify the type of an incoming JSON message.
// ============================================================

enum class MessageKind {
    // Method responses
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
    // Push messages
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
    // Method-reply messages
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
    // Push channel messages
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

// ============================================================
// Per-channel typed subscribe request aliases
//
// Usage:
//   kraken::ws::TickerSubscribeRequest req;
//   req.symbols = {"BTC/USD"};
//   auto [ack, handle] = client->subscribe(req, [](TickerMessage msg) { ... });
// ============================================================

using TickerSubscribeRequest     = TypedSubscribeRequest<TickerMessage,     SubscribeChannel::Ticker>;
using BookSubscribeRequest       = TypedSubscribeRequest<BookMessage,       SubscribeChannel::Book>;
using TradeSubscribeRequest      = TypedSubscribeRequest<TradeMessage,      SubscribeChannel::Trade>;
using OHLCSubscribeRequest       = TypedSubscribeRequest<OHLCMessage,       SubscribeChannel::OHLC>;
using InstrumentSubscribeRequest = TypedSubscribeRequest<InstrumentMessage, SubscribeChannel::Instrument>;
using ExecutionsSubscribeRequest = TypedSubscribeRequest<ExecutionsMessage,  SubscribeChannel::Executions>;
using BalancesSubscribeRequest   = TypedSubscribeRequest<BalancesMessage,   SubscribeChannel::Balances>;

} // namespace kraken::ws
