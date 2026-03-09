// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_types.hpp
// Shared enumerations, sub-objects, and order description types used by both
// the Kraken REST API layer (kraken_rest_api.hpp) and the WebSocket v2 layer
// (kraken_ws_api.hpp).
//
// Key auth model differences:
//   REST private:      nonce (uint64) + HMAC-SHA512 signature sent as headers
//                        API-Key  / API-Sign
//   WebSocket private: session token obtained via REST /GetWebSocketsToken,
//                        sent as "token" field inside each request's params.
//
// Both layers share order parameter structures (OrderType, Side, Triggers,
// Conditional, etc.).  Auth-specific fields (nonce/token) live in the layer
// that owns them.

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>
#include <map>

namespace kraken {

using json = nlohmann::json;

// ============================================================
// Response interfaces
//
// IApiResult  — base for all API result/response data types.
// IRestResult — marker interface for top-level REST result types.
//               Inherit this for every type that appears as T in
//               RestResponse<T>.
//
// These interfaces carry no behaviour in Step 1. They exist to
// establish the type hierarchy and to act as extension points for
// Step 2, where direct JSON access will be added.
// ============================================================

class IApiResult {
public:
    virtual ~IApiResult() = default;
};

class IRestResult : public IApiResult {
public:
    virtual ~IRestResult() = default;
};

// ============================================================
// Enumerations + string conversion helpers
// ============================================================

enum class OrderType {
    Limit,
    Market,
    Iceberg,
    StopLoss,
    StopLossLimit,
    TakeProfit,
    TakeProfitLimit,
    TrailingStop,
    TrailingStopLimit,
    SettlePosition
};

enum class Side { Buy, Sell };

enum class TimeInForce { GTC, GTD, IOC };

enum class PriceType { Static, Pct, Quote };

enum class TriggerReference { Index, Last };

enum class StpType { CancelNewest, CancelOldest, CancelBoth };

enum class FeePreference { Base, Quote };

enum class OrderStatus {
    PendingNew,
    New,
    PartiallyFilled,
    Filled,
    Canceled,
    Expired,
    Unknown
};

// -- to_string / from_string ------------------------------------------

inline std::string to_string(OrderType v) {
    switch (v) {
        case OrderType::Limit:             return "limit";
        case OrderType::Market:            return "market";
        case OrderType::Iceberg:           return "iceberg";
        case OrderType::StopLoss:          return "stop-loss";
        case OrderType::StopLossLimit:     return "stop-loss-limit";
        case OrderType::TakeProfit:        return "take-profit";
        case OrderType::TakeProfitLimit:   return "take-profit-limit";
        case OrderType::TrailingStop:      return "trailing-stop";
        case OrderType::TrailingStopLimit: return "trailing-stop-limit";
        case OrderType::SettlePosition:    return "settle-position";
    }
    throw std::invalid_argument("Unknown OrderType");
}
inline OrderType order_type_from_string(const std::string& s) {
    if (s == "limit")               return OrderType::Limit;
    if (s == "market")              return OrderType::Market;
    if (s == "iceberg")             return OrderType::Iceberg;
    if (s == "stop-loss")           return OrderType::StopLoss;
    if (s == "stop-loss-limit")     return OrderType::StopLossLimit;
    if (s == "take-profit")         return OrderType::TakeProfit;
    if (s == "take-profit-limit")   return OrderType::TakeProfitLimit;
    if (s == "trailing-stop")       return OrderType::TrailingStop;
    if (s == "trailing-stop-limit") return OrderType::TrailingStopLimit;
    if (s == "settle-position")     return OrderType::SettlePosition;
    throw std::invalid_argument("Unknown order_type: " + s);
}

inline std::string to_string(Side v)       { return v == Side::Buy ? "buy" : "sell"; }
inline Side side_from_string(const std::string& s) {
    if (s == "buy")  return Side::Buy;
    if (s == "sell") return Side::Sell;
    throw std::invalid_argument("Unknown side: " + s);
}

inline std::string to_string(TimeInForce v) {
    switch (v) {
        case TimeInForce::GTC: return "gtc";
        case TimeInForce::GTD: return "gtd";
        case TimeInForce::IOC: return "ioc";
    }
    throw std::invalid_argument("Unknown TimeInForce");
}
inline TimeInForce tif_from_string(const std::string& s) {
    if (s == "gtc") return TimeInForce::GTC;
    if (s == "gtd") return TimeInForce::GTD;
    if (s == "ioc") return TimeInForce::IOC;
    throw std::invalid_argument("Unknown time_in_force: " + s);
}

inline std::string to_string(PriceType v) {
    switch (v) {
        case PriceType::Static: return "static";
        case PriceType::Pct:    return "pct";
        case PriceType::Quote:  return "quote";
    }
    throw std::invalid_argument("Unknown PriceType");
}
inline PriceType price_type_from_string(const std::string& s) {
    if (s == "static") return PriceType::Static;
    if (s == "pct")    return PriceType::Pct;
    if (s == "quote")  return PriceType::Quote;
    throw std::invalid_argument("Unknown price_type: " + s);
}

inline std::string to_string(TriggerReference v) { return v == TriggerReference::Index ? "index" : "last"; }
inline TriggerReference trigger_ref_from_string(const std::string& s) {
    if (s == "index") return TriggerReference::Index;
    if (s == "last")  return TriggerReference::Last;
    throw std::invalid_argument("Unknown trigger reference: " + s);
}

inline std::string to_string(StpType v) {
    switch (v) {
        case StpType::CancelNewest: return "cancel_newest";
        case StpType::CancelOldest: return "cancel_oldest";
        case StpType::CancelBoth:   return "cancel_both";
    }
    throw std::invalid_argument("Unknown StpType");
}

inline std::string to_string(FeePreference v) { return v == FeePreference::Base ? "base" : "quote"; }

inline std::string to_string(OrderStatus v) {
    switch (v) {
        case OrderStatus::PendingNew:      return "pending_new";
        case OrderStatus::New:             return "new";
        case OrderStatus::PartiallyFilled: return "partially_filled";
        case OrderStatus::Filled:          return "filled";
        case OrderStatus::Canceled:        return "canceled";
        case OrderStatus::Expired:         return "expired";
        case OrderStatus::Unknown:         return "unknown";
    }
    return "unknown";
}
inline OrderStatus order_status_from_string(const std::string& s) {
    if (s == "pending_new" || s == "pending")  return OrderStatus::PendingNew;
    if (s == "new" || s == "open")             return OrderStatus::New;
    if (s == "partially_filled")               return OrderStatus::PartiallyFilled;
    if (s == "filled" || s == "closed")        return OrderStatus::Filled;
    if (s == "canceled" || s == "cancelled")   return OrderStatus::Canceled;
    if (s == "expired")                        return OrderStatus::Expired;
    return OrderStatus::Unknown;
}

// ============================================================
// Sub-objects shared between REST and WebSocket
// ============================================================

// Trigger section for stop/trailing order types.
class Triggers {
public:
    double price{0.0};
    std::optional<TriggerReference> reference;  // default: last
    std::optional<PriceType>        price_type; // default: static

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

// Conditional secondary (OTO) close order.
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
// Core order parameter block
// Shared by AddOrderRequest (REST + WS) and BatchOrder entries.
// Auth fields (token / nonce) are NOT in here – they live in the
// protocol-specific request wrappers.
// ============================================================

class OrderParams {
public:
    // Required
    OrderType   order_type{OrderType::Market};
    Side        side{Side::Buy};
    double      order_qty{0.0};
    std::string symbol;       // e.g. "BTC/USD"  (WS) or "XBTUSD" (REST pair field)

    // Optional pricing
    std::optional<double>    limit_price;
    std::optional<PriceType> limit_price_type;

    // Trigger section
    std::optional<Triggers>   triggers;

    // OTO conditional close
    std::optional<Conditional> conditional;

    // Execution controls
    std::optional<TimeInForce>   time_in_force;
    std::optional<bool>          margin;
    std::optional<bool>          post_only;
    std::optional<bool>          reduce_only;
    std::optional<std::string>   effective_time;   // RFC3339
    std::optional<std::string>   expire_time;      // RFC3339 (GTD)
    std::optional<std::string>   deadline;         // RFC3339, max 60s ahead
    std::optional<std::string>   cl_ord_id;
    std::optional<int64_t>       order_userref;
    std::optional<double>        display_qty;      // iceberg
    std::optional<FeePreference> fee_preference;
    std::optional<StpType>       stp_type;
    std::optional<double>        cash_order_qty;   // buy market without margin
    std::optional<bool>          validate;
    std::optional<std::string>   sender_sub_id;

    // Serialise the common order fields into an existing json object.
    // The caller adds auth fields (token / nonce) and the outer method wrapper.
    json to_json() const {
        json j;
        j["order_type"] = to_string(order_type);
        j["side"]       = to_string(side);
        j["order_qty"]  = order_qty;
        // symbol key differs: WS uses "symbol", REST uses "pair" – callers override if needed
        j["symbol"]     = symbol;

        if (limit_price)      j["limit_price"]      = *limit_price;
        if (limit_price_type) j["limit_price_type"] = to_string(*limit_price_type);
        if (triggers)         j["triggers"]         = triggers->to_json();
        if (conditional)      j["conditional"]      = conditional->to_json();
        if (time_in_force)    j["time_in_force"]    = to_string(*time_in_force);
        if (margin)           j["margin"]           = *margin;
        if (post_only)        j["post_only"]        = *post_only;
        if (reduce_only)      j["reduce_only"]      = *reduce_only;
        if (effective_time)   j["effective_time"]   = *effective_time;
        if (expire_time)      j["expire_time"]      = *expire_time;
        if (deadline)         j["deadline"]         = *deadline;
        if (cl_ord_id)        j["cl_ord_id"]        = *cl_ord_id;
        if (order_userref)    j["order_userref"]    = *order_userref;
        if (display_qty)      j["display_qty"]      = *display_qty;
        if (fee_preference)   j["fee_preference"]   = to_string(*fee_preference);
        if (stp_type)         j["stp_type"]         = to_string(*stp_type);
        if (cash_order_qty)   j["cash_order_qty"]   = *cash_order_qty;
        if (validate)         j["validate"]         = *validate;
        if (sender_sub_id)    j["sender_sub_id"]    = *sender_sub_id;
        return j;
    }

    static OrderParams from_json(const json& j) {
        OrderParams p;
        if (j.contains("order_type"))  p.order_type = order_type_from_string(j["order_type"].get<std::string>());
        if (j.contains("side"))        p.side       = side_from_string(j["side"].get<std::string>());
        if (j.contains("order_qty"))   p.order_qty  = j["order_qty"].get<double>();
        // Accept both "symbol" (WS) and "pair" (REST)
        if (j.contains("symbol"))      p.symbol = j["symbol"].get<std::string>();
        else if (j.contains("pair"))   p.symbol = j["pair"].get<std::string>();

        if (j.contains("limit_price"))      p.limit_price      = j["limit_price"].get<double>();
        if (j.contains("limit_price_type")) p.limit_price_type = price_type_from_string(j["limit_price_type"].get<std::string>());
        if (j.contains("triggers"))         p.triggers         = Triggers::from_json(j["triggers"]);
        if (j.contains("conditional"))      p.conditional      = Conditional::from_json(j["conditional"]);
        if (j.contains("time_in_force"))    p.time_in_force    = tif_from_string(j["time_in_force"].get<std::string>());
        if (j.contains("margin"))           p.margin           = j["margin"].get<bool>();
        if (j.contains("post_only"))        p.post_only        = j["post_only"].get<bool>();
        if (j.contains("reduce_only"))      p.reduce_only      = j["reduce_only"].get<bool>();
        if (j.contains("effective_time"))   p.effective_time   = j["effective_time"].get<std::string>();
        if (j.contains("expire_time"))      p.expire_time      = j["expire_time"].get<std::string>();
        if (j.contains("deadline"))         p.deadline         = j["deadline"].get<std::string>();
        if (j.contains("cl_ord_id"))        p.cl_ord_id        = j["cl_ord_id"].get<std::string>();
        if (j.contains("order_userref"))    p.order_userref    = j["order_userref"].get<int64_t>();
        if (j.contains("display_qty"))      p.display_qty      = j["display_qty"].get<double>();
        if (j.contains("fee_preference"))   p.fee_preference   = (j["fee_preference"].get<std::string>() == "base") ? FeePreference::Base : FeePreference::Quote;
        if (j.contains("stp_type"))         p.stp_type         = StpType::CancelNewest; // parse if needed
        if (j.contains("cash_order_qty"))   p.cash_order_qty   = j["cash_order_qty"].get<double>();
        if (j.contains("validate"))         p.validate         = j["validate"].get<bool>();
        if (j.contains("sender_sub_id"))    p.sender_sub_id    = j["sender_sub_id"].get<std::string>();
        return p;
    }
};

// ============================================================
// Order description – returned by GET open/closed order endpoints
// and by add-order responses (both REST and WS executions feed).
// ============================================================

class OrderDescription {
private:
    std::string pair_;
    Side        side_{Side::Buy};
    OrderType   order_type_{OrderType::Market};
    std::string price_;    // kept as string to preserve precision
    std::string price2_;
    std::string leverage_;
    std::string order_;    // human readable description
    std::string close_;

public:
    const std::string& pair()      const { return pair_; }
    Side               side()      const { return side_; }
    OrderType          order_type() const { return order_type_; }
    const std::string& price()     const { return price_; }
    const std::string& price2()    const { return price2_; }
    const std::string& leverage()  const { return leverage_; }
    const std::string& order()     const { return order_; }
    const std::string& close()     const { return close_; }

    static OrderDescription from_json(const json& j) {
        OrderDescription d;
        d.pair_       = j.value("pair", "");
        d.price_      = j.value("price", "");
        d.price2_     = j.value("price2", "");
        d.leverage_   = j.value("leverage", "");
        d.order_      = j.value("order", "");
        d.close_      = j.value("close", "");
        if (j.contains("type"))      d.side_       = side_from_string(j["type"].get<std::string>());
        if (j.contains("ordertype")) d.order_type_ = order_type_from_string(j["ordertype"].get<std::string>());
        return d;
    }
};

// ============================================================
// Full order info – used by GetOpenOrders, GetClosedOrders, QueryOrdersInfo
// ============================================================

class OrderInfo {
private:
    std::string       txid_;           // set by caller from map key
    OrderStatus       status_{OrderStatus::Unknown};
    OrderDescription  descr_;
    double            vol_{0.0};
    double            vol_exec_{0.0};
    double            cost_{0.0};
    double            fee_{0.0};
    double            price_{0.0};     // avg price
    double            stopprice_{0.0};
    double            limitprice_{0.0};
    std::string       misc_;
    std::string       oflags_;
    std::optional<int64_t>     userref_;
    std::optional<double>      opentm_;
    std::optional<double>      closetm_;
    std::optional<double>      starttm_;
    std::optional<double>      expiretm_;
    std::optional<std::string> reason_;
    std::optional<std::vector<std::string>> trades_; // trade ids

public:
    const std::string&      txid()       const { return txid_; }
    OrderStatus             status()     const { return status_; }
    const OrderDescription& descr()      const { return descr_; }
    double                  vol()        const { return vol_; }
    double                  vol_exec()   const { return vol_exec_; }
    double                  cost()       const { return cost_; }
    double                  fee()        const { return fee_; }
    double                  price()      const { return price_; }
    double                  stopprice()  const { return stopprice_; }
    double                  limitprice() const { return limitprice_; }
    const std::string&      misc()       const { return misc_; }
    const std::string&      oflags()     const { return oflags_; }
    std::optional<int64_t>  userref()    const { return userref_; }
    std::optional<double>   opentm()     const { return opentm_; }
    std::optional<double>   closetm()    const { return closetm_; }
    std::optional<double>   starttm()    const { return starttm_; }
    std::optional<double>   expiretm()   const { return expiretm_; }
    std::optional<std::string> reason()  const { return reason_; }
    std::optional<std::vector<std::string>> trades() const { return trades_; }

    static OrderInfo from_json(const json& j, const std::string& id = "") {
        OrderInfo o;
        o.txid_      = id;
        o.vol_       = std::stod(j.value("vol", "0"));
        o.vol_exec_  = std::stod(j.value("vol_exec", "0"));
        o.cost_      = std::stod(j.value("cost", "0"));
        o.fee_       = std::stod(j.value("fee", "0"));
        o.price_     = std::stod(j.value("price", "0"));
        o.stopprice_ = std::stod(j.value("stopprice", "0"));
        o.limitprice_= std::stod(j.value("limitprice", "0"));
        o.misc_      = j.value("misc", "");
        o.oflags_    = j.value("oflags", "");
        if (j.contains("status"))   o.status_  = order_status_from_string(j["status"].get<std::string>());
        if (j.contains("descr"))    o.descr_   = OrderDescription::from_json(j["descr"]);
        if (j.contains("userref"))  o.userref_ = j["userref"].get<int64_t>();
        if (j.contains("opentm"))   o.opentm_  = j["opentm"].get<double>();
        if (j.contains("closetm"))  o.closetm_ = j["closetm"].get<double>();
        if (j.contains("starttm"))  o.starttm_ = j["starttm"].get<double>();
        if (j.contains("expiretm")) o.expiretm_= j["expiretm"].get<double>();
        if (j.contains("reason"))   o.reason_  = j["reason"].get<std::string>();
        if (j.contains("trades"))   o.trades_  = j["trades"].get<std::vector<std::string>>();
        return o;
    }
};

// ============================================================
// Trade info – used by GetTradesHistory, QueryTradesInfo
// ============================================================

class TradeInfo {
private:
    std::string txid_;
    std::string ordertxid_;
    std::string pair_;
    double      time_{0.0};
    Side        type_{Side::Buy};
    OrderType   ordertype_{OrderType::Market};
    double      price_{0.0};
    double      cost_{0.0};
    double      fee_{0.0};
    double      vol_{0.0};
    double      margin_{0.0};
    std::string misc_;
    std::optional<std::string> posstatus_;
    std::optional<double>      cprice_;
    std::optional<double>      ccost_;
    std::optional<double>      cfee_;
    std::optional<double>      cvol_;
    std::optional<double>      cmargin_;
    std::optional<double>      net_;

public:
    const std::string& txid()      const { return txid_; }
    const std::string& ordertxid() const { return ordertxid_; }
    const std::string& pair()      const { return pair_; }
    double             time()      const { return time_; }
    Side               type()      const { return type_; }
    OrderType          ordertype() const { return ordertype_; }
    double             price()     const { return price_; }
    double             cost()      const { return cost_; }
    double             fee()       const { return fee_; }
    double             vol()       const { return vol_; }
    double             margin()    const { return margin_; }
    const std::string& misc()      const { return misc_; }
    std::optional<std::string> posstatus() const { return posstatus_; }
    std::optional<double>      cprice()    const { return cprice_; }
    std::optional<double>      ccost()     const { return ccost_; }
    std::optional<double>      cfee()      const { return cfee_; }
    std::optional<double>      cvol()      const { return cvol_; }
    std::optional<double>      cmargin()   const { return cmargin_; }
    std::optional<double>      net()       const { return net_; }

    static TradeInfo from_json(const json& j, const std::string& id = "") {
        TradeInfo t;
        t.txid_     = id;
        t.ordertxid_= j.value("ordertxid", "");
        t.pair_     = j.value("pair", "");
        t.time_     = j.value("time", 0.0);
        t.price_    = std::stod(j.value("price", "0"));
        t.cost_     = std::stod(j.value("cost", "0"));
        t.fee_      = std::stod(j.value("fee", "0"));
        t.vol_      = std::stod(j.value("vol", "0"));
        t.margin_   = std::stod(j.value("margin", "0"));
        t.misc_     = j.value("misc", "");
        if (j.contains("type"))      t.type_      = side_from_string(j["type"].get<std::string>());
        if (j.contains("ordertype")) t.ordertype_ = order_type_from_string(j["ordertype"].get<std::string>());
        if (j.contains("posstatus")) t.posstatus_ = j["posstatus"].get<std::string>();
        if (j.contains("cprice"))    t.cprice_    = std::stod(j["cprice"].get<std::string>());
        if (j.contains("ccost"))     t.ccost_     = std::stod(j["ccost"].get<std::string>());
        if (j.contains("cfee"))      t.cfee_      = std::stod(j["cfee"].get<std::string>());
        if (j.contains("cvol"))      t.cvol_      = std::stod(j["cvol"].get<std::string>());
        if (j.contains("cmargin"))   t.cmargin_   = std::stod(j["cmargin"].get<std::string>());
        if (j.contains("net"))       t.net_       = std::stod(j["net"].get<std::string>());
        return t;
    }
};

// ============================================================
// Ledger entry
// ============================================================

class LedgerEntry {
private:
    std::string txid_;
    std::string refid_;
    double      time_{0.0};
    std::string type_;
    std::string subtype_;
    std::string aclass_;
    std::string asset_;
    double      amount_{0.0};
    double      fee_{0.0};
    double      balance_{0.0};

public:
    const std::string& txid()    const { return txid_; }
    const std::string& refid()   const { return refid_; }
    double             time()    const { return time_; }
    const std::string& type()    const { return type_; }
    const std::string& subtype() const { return subtype_; }
    const std::string& aclass()  const { return aclass_; }
    const std::string& asset()   const { return asset_; }
    double             amount()  const { return amount_; }
    double             fee()     const { return fee_; }
    double             balance() const { return balance_; }

    static LedgerEntry from_json(const json& j, const std::string& id = "") {
        LedgerEntry e;
        e.txid_    = id;
        e.refid_   = j.value("refid", "");
        e.time_    = j.value("time", 0.0);
        e.type_    = j.value("type", "");
        e.subtype_ = j.value("subtype", "");
        e.aclass_  = j.value("aclass", "");
        e.asset_   = j.value("asset", "");
        e.amount_  = std::stod(j.value("amount", "0"));
        e.fee_     = std::stod(j.value("fee", "0"));
        e.balance_ = std::stod(j.value("balance", "0"));
        return e;
    }
};

// ============================================================
// Generic REST envelope  { "error": [], "result": <T> }
// ============================================================

template<typename T>
struct RestResponse {
    std::vector<std::string> errors;
    bool                     ok{false};
    std::optional<T>         result;

    bool has_error() const { return !errors.empty(); }
    const std::string& first_error() const {
        static const std::string none;
        return errors.empty() ? none : errors[0];
    }
};

// Helper: parse the outer envelope and call T::from_json(result_node)
template<typename T>
RestResponse<T> parse_rest_response(const json& j) {
    RestResponse<T> r;
    if (j.contains("error")) {
        for (const auto& e : j["error"])
            r.errors.push_back(e.get<std::string>());
    }
    r.ok = r.errors.empty();
    if (r.ok && j.contains("result"))
        r.result = T::from_json(j["result"]);
    return r;
}

} // namespace kraken
