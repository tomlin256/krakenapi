// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/kraken/rest_api.hpp
// Kraken Spot REST API – request builders and response parsers.
//
// Namespace: exchange::kraken::rest

#include "exchange/kraken/auth.hpp"
#include "exchange/kraken/types.hpp"
#include "exchange/common/rest.hpp"

#include <nlohmann/json.hpp>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace exchange::kraken::rest {

using json = nlohmann::json;

// Re-export common scaffold types so callers only need this header.
using exchange::rest::HttpRequest;

// ── Request base classes ──────────────────────────────────────────────────────

// A Kraken public request (no auth needed).
struct PublicRequest {
    virtual ~PublicRequest() = default;
    virtual HttpRequest build() const = 0;
};

// A Kraken private request (signing required).
struct PrivateRequest {
    virtual ~PrivateRequest() = default;
    virtual HttpRequest build(const Credentials& creds) const = 0;

protected:
    HttpRequest make_private_request(const std::string& uri_path,
                                     std::map<std::string, std::string> params,
                                     const Credentials& creds) const {
        std::string nonce_str = std::to_string(make_nonce());
        params["nonce"] = nonce_str;
        std::string body = detail::build_form_body(params);
        std::string sign = creds.sign(uri_path, nonce_str, body);

        HttpRequest req;
        req.method = HttpRequest::Method::POST;
        req.path   = uri_path;
        req.body   = body;
        req.headers["Content-Type"] = "application/x-www-form-urlencoded";
        req.headers["API-Key"]      = creds.api_key;
        req.headers["API-Sign"]     = sign;
        return req;
    }
};

// Typed bases — link each request type to its response type at compile time.
template<typename R>
struct TypedPublicRequest : PublicRequest {
    using response_type = R;
};

template<typename R>
struct TypedPrivateRequest : PrivateRequest {
    using response_type = R;
};

// ── Forward declarations ──────────────────────────────────────────────────────

struct ServerTime;
struct SystemStatus;
struct AssetInfoResult;
struct AssetPairsResult;
struct TickerResult;
struct OHLCResult;
struct OrderBookResult;
struct RecentTradesResult;
struct AccountBalanceResult;
struct ExtendedBalanceResult;
struct TradeBalance;
struct OpenOrdersResult;
struct ClosedOrdersResult;
struct QueryOrdersResultWrapper;
struct TradesHistoryResult;
struct QueryTradesResultWrapper;
struct OpenPositionsResult;
struct LedgersResult;
struct QueryLedgersResultWrapper;
struct AddOrderResult;
struct AddOrderBatchResult;
struct EditOrderResult;
struct AmendOrderResult;
struct CancelOrderResult;
struct CancelAllResult;
struct CancelAllAfterResult;
struct CancelOrderBatchResult;
struct WebSocketsTokenResult;
struct DepositMethodsResult;
struct DepositAddressesResult;
struct WithdrawResult;
struct CancelWithdrawalResult;
struct CreateSubaccountResult;
struct EarnBoolResult;

// ── MARKET DATA (public) ──────────────────────────────────────────────────────

struct GetServerTimeRequest : TypedPublicRequest<ServerTime> {
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Time";
        return r;
    }
};

struct ServerTime {
    int64_t     unixtime{0};
    std::string rfc1123;
    static ServerTime from_json(const json& j) {
        ServerTime t;
        t.unixtime = j.value("unixtime", int64_t{0});
        t.rfc1123  = j.value("rfc1123", "");
        return t;
    }
};

struct GetSystemStatusRequest : TypedPublicRequest<SystemStatus> {
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/SystemStatus";
        return r;
    }
};

struct SystemStatus {
    std::string status;
    std::string timestamp;
    static SystemStatus from_json(const json& j) {
        SystemStatus s;
        s.status    = j.value("status", "");
        s.timestamp = j.value("timestamp", "");
        return s;
    }
};

struct GetAssetInfoRequest : TypedPublicRequest<AssetInfoResult> {
    std::optional<std::vector<std::string>> assets;
    std::optional<std::string>              aclass;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Assets";
        std::string q;
        if (assets && !assets->empty()) {
            q += "asset=";
            for (size_t i = 0; i < assets->size(); ++i) {
                if (i) q += ',';
                q += (*assets)[i];
            }
        }
        if (aclass) { if (!q.empty()) q += '&'; q += "aclass=" + *aclass; }
        r.query = q;
        return r;
    }
};

struct AssetInfo {
    std::string aclass;
    std::string altname;
    int         decimals{0};
    int         display_decimals{0};
    static AssetInfo from_json(const json& j) {
        AssetInfo a;
        a.aclass           = j.value("aclass", "");
        a.altname          = j.value("altname", "");
        a.decimals         = j.value("decimals", 0);
        a.display_decimals = j.value("display_decimals", 0);
        return a;
    }
};

struct AssetInfoResult {
    std::map<std::string, AssetInfo> assets;
    static AssetInfoResult from_json(const json& j) {
        AssetInfoResult r;
        for (const auto& [k, v] : j.items())
            r.assets[k] = AssetInfo::from_json(v);
        return r;
    }
};

struct GetAssetPairsRequest : TypedPublicRequest<AssetPairsResult> {
    std::optional<std::vector<std::string>> pairs;
    std::optional<std::string>              info;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/AssetPairs";
        std::string q;
        if (pairs && !pairs->empty()) {
            q += "pair=";
            for (size_t i = 0; i < pairs->size(); ++i) { if (i) q += ','; q += (*pairs)[i]; }
        }
        if (info) { if (!q.empty()) q += '&'; q += "info=" + *info; }
        r.query = q;
        return r;
    }
};

struct AssetPairInfo {
    std::string altname;
    std::string wsname;
    std::string base;
    std::string quote;
    int         pair_decimals{0};
    int         lot_decimals{0};
    double      ordermin{0.0};
    double      costmin{0.0};
    std::vector<std::vector<double>> fees;
    std::vector<std::vector<double>> fees_maker;

    static AssetPairInfo from_json(const json& j) {
        AssetPairInfo p;
        p.altname       = j.value("altname", "");
        p.wsname        = j.value("wsname", "");
        p.base          = j.value("base", "");
        p.quote         = j.value("quote", "");
        p.pair_decimals = j.value("pair_decimals", 0);
        p.lot_decimals  = j.value("lot_decimals", 0);
        if (j.contains("ordermin")) p.ordermin = std::stod(j["ordermin"].get<std::string>());
        if (j.contains("costmin"))  p.costmin  = std::stod(j["costmin"].get<std::string>());
        return p;
    }
};

struct AssetPairsResult {
    std::map<std::string, AssetPairInfo> pairs;
    static AssetPairsResult from_json(const json& j) {
        AssetPairsResult r;
        for (const auto& [k, v] : j.items())
            r.pairs[k] = AssetPairInfo::from_json(v);
        return r;
    }
};

struct GetTickerRequest : TypedPublicRequest<TickerResult> {
    std::optional<std::vector<std::string>> pairs;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Ticker";
        if (pairs && !pairs->empty()) {
            r.query = "pair=";
            for (size_t i = 0; i < pairs->size(); ++i) { if (i) r.query += ','; r.query += (*pairs)[i]; }
        }
        return r;
    }
};

struct TickerInfo {
    double ask{0.0};
    double bid{0.0};
    double last{0.0};
    double volume_today{0.0};
    double volume_24h{0.0};
    double vwap_today{0.0};
    double vwap_24h{0.0};
    int64_t trades_today{0};
    int64_t trades_24h{0};
    double  low_today{0.0};
    double  low_24h{0.0};
    double  high_today{0.0};
    double  high_24h{0.0};
    double  open{0.0};

    static TickerInfo from_json(const json& j) {
        TickerInfo t;
        if (j.contains("a")) t.ask          = std::stod(j["a"][0].get<std::string>());
        if (j.contains("b")) t.bid          = std::stod(j["b"][0].get<std::string>());
        if (j.contains("c")) t.last         = std::stod(j["c"][0].get<std::string>());
        if (j.contains("v")) { t.volume_today = std::stod(j["v"][0].get<std::string>()); t.volume_24h = std::stod(j["v"][1].get<std::string>()); }
        if (j.contains("p")) { t.vwap_today   = std::stod(j["p"][0].get<std::string>()); t.vwap_24h   = std::stod(j["p"][1].get<std::string>()); }
        if (j.contains("t")) { t.trades_today = j["t"][0].get<int64_t>();                t.trades_24h = j["t"][1].get<int64_t>(); }
        if (j.contains("l")) { t.low_today    = std::stod(j["l"][0].get<std::string>()); t.low_24h    = std::stod(j["l"][1].get<std::string>()); }
        if (j.contains("h")) { t.high_today   = std::stod(j["h"][0].get<std::string>()); t.high_24h   = std::stod(j["h"][1].get<std::string>()); }
        if (j.contains("o")) t.open          = std::stod(j["o"].get<std::string>());
        return t;
    }
};

struct TickerResult {
    std::map<std::string, TickerInfo> tickers;
    static TickerResult from_json(const json& j) {
        TickerResult r;
        for (const auto& [k, v] : j.items())
            r.tickers[k] = TickerInfo::from_json(v);
        return r;
    }
};

struct GetOHLCRequest : TypedPublicRequest<OHLCResult> {
    std::string pair;
    std::optional<int32_t>  interval;
    std::optional<int64_t>  since;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/OHLC";
        r.query  = "pair=" + pair;
        if (interval) r.query += "&interval=" + std::to_string(*interval);
        if (since)    r.query += "&since=" + std::to_string(*since);
        return r;
    }
};

struct OHLCCandle {
    int64_t time{0};
    double  open{0.0};
    double  high{0.0};
    double  low{0.0};
    double  close{0.0};
    double  vwap{0.0};
    double  volume{0.0};
    int64_t count{0};
};

struct OHLCResult {
    std::string              pair;
    std::vector<OHLCCandle>  candles;
    int64_t                  last{0};

    static OHLCResult from_json(const json& j) {
        OHLCResult r;
        for (const auto& [k, v] : j.items()) {
            if (k == "last") { r.last = v.get<int64_t>(); continue; }
            r.pair = k;
            for (const auto& c : v) {
                OHLCCandle candle;
                candle.time   = c[0].get<int64_t>();
                candle.open   = std::stod(c[1].get<std::string>());
                candle.high   = std::stod(c[2].get<std::string>());
                candle.low    = std::stod(c[3].get<std::string>());
                candle.close  = std::stod(c[4].get<std::string>());
                candle.vwap   = std::stod(c[5].get<std::string>());
                candle.volume = std::stod(c[6].get<std::string>());
                candle.count  = c[7].get<int64_t>();
                r.candles.push_back(candle);
            }
        }
        return r;
    }
};

struct GetOrderBookRequest : TypedPublicRequest<OrderBookResult> {
    std::string pair;
    std::optional<int32_t> count;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Depth";
        r.query  = "pair=" + pair;
        if (count) r.query += "&count=" + std::to_string(*count);
        return r;
    }
};

struct RestBookEntry { double price{0.0}; double volume{0.0}; int64_t timestamp{0}; };

struct OrderBookResult {
    std::string                 pair;
    std::vector<RestBookEntry>  asks;
    std::vector<RestBookEntry>  bids;

    static OrderBookResult from_json(const json& j) {
        OrderBookResult r;
        for (const auto& [k, v] : j.items()) {
            r.pair = k;
            auto parse = [](const json& arr) {
                std::vector<RestBookEntry> entries;
                for (const auto& e : arr)
                    entries.push_back({std::stod(e[0].get<std::string>()),
                                       std::stod(e[1].get<std::string>()),
                                       e[2].get<int64_t>()});
                return entries;
            };
            r.asks = parse(v["asks"]);
            r.bids = parse(v["bids"]);
        }
        return r;
    }
};

struct GetRecentTradesRequest : TypedPublicRequest<RecentTradesResult> {
    std::string pair;
    std::optional<int64_t>  since;
    std::optional<int32_t>  count;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Trades";
        r.query  = "pair=" + pair;
        if (since) r.query += "&since=" + std::to_string(*since);
        if (count) r.query += "&count=" + std::to_string(*count);
        return r;
    }
};

struct PublicTrade {
    double      price{0.0};
    double      volume{0.0};
    double      time{0.0};
    Side        side{Side::Buy};
    std::string order_type;
    std::string misc;
};

struct RecentTradesResult {
    std::string              pair;
    std::vector<PublicTrade> trades;
    std::string              last;

    static RecentTradesResult from_json(const json& j) {
        RecentTradesResult r;
        for (const auto& [k, v] : j.items()) {
            if (k == "last") { r.last = v.get<std::string>(); continue; }
            r.pair = k;
            for (const auto& t : v) {
                PublicTrade pt;
                pt.price      = std::stod(t[0].get<std::string>());
                pt.volume     = std::stod(t[1].get<std::string>());
                pt.time       = t[2].get<double>();
                pt.side       = (t[3].get<std::string>() == "b") ? Side::Buy : Side::Sell;
                pt.order_type = t[4].get<std::string>();
                pt.misc       = t[5].get<std::string>();
                r.trades.push_back(pt);
            }
        }
        return r;
    }
};

// ── ACCOUNT DATA (private) ────────────────────────────────────────────────────

struct GetAccountBalanceRequest : TypedPrivateRequest<AccountBalanceResult> {
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/Balance", {}, creds);
    }
};

struct AccountBalanceResult {
    std::map<std::string, double> balances;
    static AccountBalanceResult from_json(const json& j) {
        AccountBalanceResult r;
        for (const auto& [k, v] : j.items())
            r.balances[k] = std::stod(v.get<std::string>());
        return r;
    }
};

struct GetExtendedBalanceRequest : TypedPrivateRequest<ExtendedBalanceResult> {
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/BalanceEx", {}, creds);
    }
};

struct ExtendedBalance {
    double balance{0.0};
    double hold_trade{0.0};
    double credit{0.0};
    double credit_used{0.0};
};

struct ExtendedBalanceResult {
    std::map<std::string, ExtendedBalance> balances;
    static ExtendedBalanceResult from_json(const json& j) {
        ExtendedBalanceResult r;
        for (const auto& [k, v] : j.items()) {
            ExtendedBalance b;
            b.balance     = std::stod(v.value("balance", "0"));
            b.hold_trade  = std::stod(v.value("hold_trade", "0"));
            b.credit      = std::stod(v.value("credit", "0"));
            b.credit_used = std::stod(v.value("credit_used", "0"));
            r.balances[k] = b;
        }
        return r;
    }
};

struct GetTradeBalanceRequest : TypedPrivateRequest<TradeBalance> {
    std::optional<std::string> asset;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (asset) p["asset"] = *asset;
        return make_private_request("/0/private/TradeBalance", p, creds);
    }
};

struct TradeBalance {
    double eb{0.0};
    double tb{0.0};
    double m{0.0};
    double n{0.0};
    double c{0.0};
    double v{0.0};
    double e{0.0};
    double mf{0.0};
    std::optional<double> ml;

    static TradeBalance from_json(const json& j) {
        auto d = [&](const char* k) { return j.contains(k) ? std::stod(j[k].get<std::string>()) : 0.0; };
        TradeBalance t;
        t.eb = d("eb"); t.tb = d("tb"); t.m = d("m"); t.n = d("n");
        t.c  = d("c");  t.v  = d("v"); t.e = d("e"); t.mf = d("mf");
        if (j.contains("ml")) t.ml = std::stod(j["ml"].get<std::string>());
        return t;
    }
};

struct GetOpenOrdersRequest : TypedPrivateRequest<OpenOrdersResult> {
    std::optional<bool>    trades;
    std::optional<int64_t> userref;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (trades && *trades)  p["trades"]  = "true";
        if (userref)            p["userref"] = std::to_string(*userref);
        return make_private_request("/0/private/OpenOrders", p, creds);
    }
};

struct OpenOrdersResult {
    std::map<std::string, exchange::kraken::OrderInfo> open;
    static OpenOrdersResult from_json(const json& j) {
        OpenOrdersResult r;
        if (j.contains("open"))
            for (const auto& [k, v] : j["open"].items())
                r.open[k] = exchange::kraken::OrderInfo::from_json(v, k);
        return r;
    }
};

struct GetClosedOrdersRequest : TypedPrivateRequest<ClosedOrdersResult> {
    std::optional<bool>    trades;
    std::optional<int64_t> userref;
    std::optional<double>  start;
    std::optional<double>  end;
    std::optional<int32_t> ofs;
    std::optional<std::string> closetime;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (trades && *trades) p["trades"]    = "true";
        if (userref)           p["userref"]   = std::to_string(*userref);
        if (start)             p["start"]     = std::to_string(*start);
        if (end)               p["end"]       = std::to_string(*end);
        if (ofs)               p["ofs"]       = std::to_string(*ofs);
        if (closetime)         p["closetime"] = *closetime;
        return make_private_request("/0/private/ClosedOrders", p, creds);
    }
};

struct ClosedOrdersResult {
    std::map<std::string, exchange::kraken::OrderInfo> closed;
    int32_t count{0};
    static ClosedOrdersResult from_json(const json& j) {
        ClosedOrdersResult r;
        if (j.contains("closed"))
            for (const auto& [k, v] : j["closed"].items())
                r.closed[k] = exchange::kraken::OrderInfo::from_json(v, k);
        if (j.contains("count")) r.count = j["count"].get<int32_t>();
        return r;
    }
};

struct QueryOrdersRequest : TypedPrivateRequest<QueryOrdersResultWrapper> {
    std::vector<std::string> txids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        std::string ids;
        for (size_t i = 0; i < txids.size(); ++i) { if (i) ids += ','; ids += txids[i]; }
        p["txid"] = ids;
        if (trades && *trades) p["trades"] = "true";
        return make_private_request("/0/private/QueryOrders", p, creds);
    }
};

struct QueryOrdersResultWrapper {
    std::map<std::string, exchange::kraken::OrderInfo> orders;
    static QueryOrdersResultWrapper from_json(const json& j) {
        QueryOrdersResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.orders[k] = exchange::kraken::OrderInfo::from_json(v, k);
        return r;
    }
};

struct GetTradesHistoryRequest : TypedPrivateRequest<TradesHistoryResult> {
    std::optional<std::string> type;
    std::optional<bool>        trades;
    std::optional<double>      start;
    std::optional<double>      end;
    std::optional<int32_t>     ofs;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (type)              p["type"]   = *type;
        if (trades && *trades) p["trades"] = "true";
        if (start)             p["start"]  = std::to_string(*start);
        if (end)               p["end"]    = std::to_string(*end);
        if (ofs)               p["ofs"]    = std::to_string(*ofs);
        return make_private_request("/0/private/TradesHistory", p, creds);
    }
};

struct TradesHistoryResult {
    std::map<std::string, exchange::kraken::TradeInfo> trades;
    int32_t count{0};
    static TradesHistoryResult from_json(const json& j) {
        TradesHistoryResult r;
        if (j.contains("trades"))
            for (const auto& [k, v] : j["trades"].items())
                r.trades[k] = exchange::kraken::TradeInfo::from_json(v, k);
        if (j.contains("count")) r.count = j["count"].get<int32_t>();
        return r;
    }
};

struct QueryTradesRequest : TypedPrivateRequest<QueryTradesResultWrapper> {
    std::vector<std::string> txids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        std::string ids;
        for (size_t i = 0; i < txids.size(); ++i) { if (i) ids += ','; ids += txids[i]; }
        p["txid"] = ids;
        if (trades && *trades) p["trades"] = "true";
        return make_private_request("/0/private/QueryTrades", p, creds);
    }
};

struct QueryTradesResultWrapper {
    std::map<std::string, exchange::kraken::TradeInfo> trades;
    static QueryTradesResultWrapper from_json(const json& j) {
        QueryTradesResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.trades[k] = exchange::kraken::TradeInfo::from_json(v, k);
        return r;
    }
};

struct GetOpenPositionsRequest : TypedPrivateRequest<OpenPositionsResult> {
    std::optional<std::vector<std::string>> txids;
    std::optional<bool> docalcs;
    std::optional<bool> consolidation;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (txids && !txids->empty()) {
            std::string ids;
            for (size_t i = 0; i < txids->size(); ++i) { if (i) ids += ','; ids += (*txids)[i]; }
            p["txid"] = ids;
        }
        if (docalcs && *docalcs)             p["docalcs"]       = "true";
        if (consolidation && *consolidation) p["consolidation"] = "market";
        return make_private_request("/0/private/OpenPositions", p, creds);
    }
};

struct PositionInfo {
    std::string ordertxid;
    std::string pair;
    double      time{0.0};
    Side        type{Side::Buy};
    OrderType   ordertype{OrderType::Market};
    double      cost{0.0};
    double      fee{0.0};
    double      vol{0.0};
    double      vol_closed{0.0};
    double      margin{0.0};
    double      value{0.0};
    double      net{0.0};
    std::string terms;
    std::string rollovertm;
    std::string misc;
    std::string oflags;

    static PositionInfo from_json(const json& j) {
        PositionInfo p;
        p.ordertxid  = j.value("ordertxid", "");
        p.pair       = j.value("pair", "");
        p.time       = j.value("time", 0.0);
        p.cost       = std::stod(j.value("cost", "0"));
        p.fee        = std::stod(j.value("fee", "0"));
        p.vol        = std::stod(j.value("vol", "0"));
        p.vol_closed = std::stod(j.value("vol_closed", "0"));
        p.margin     = std::stod(j.value("margin", "0"));
        p.terms      = j.value("terms", "");
        p.rollovertm = j.value("rollovertm", "");
        p.misc       = j.value("misc", "");
        p.oflags     = j.value("oflags", "");
        if (j.contains("type"))      p.type      = side_from_string(j["type"].get<std::string>());
        if (j.contains("ordertype")) p.ordertype = kraken_order_type_from_string(j["ordertype"].get<std::string>());
        if (j.contains("value"))     p.value     = std::stod(j["value"].get<std::string>());
        if (j.contains("net"))       p.net       = std::stod(j["net"].get<std::string>());
        return p;
    }
};

struct OpenPositionsResult {
    std::map<std::string, PositionInfo> positions;
    static OpenPositionsResult from_json(const json& j) {
        OpenPositionsResult r;
        for (const auto& [k, v] : j.items())
            r.positions[k] = PositionInfo::from_json(v);
        return r;
    }
};

struct GetLedgersRequest : TypedPrivateRequest<LedgersResult> {
    std::optional<std::vector<std::string>> assets;
    std::optional<std::string> aclass;
    std::optional<std::string> type;
    std::optional<double>  start;
    std::optional<double>  end;
    std::optional<int32_t> ofs;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (assets && !assets->empty()) {
            std::string a;
            for (size_t i = 0; i < assets->size(); ++i) { if (i) a += ','; a += (*assets)[i]; }
            p["asset"] = a;
        }
        if (aclass) p["aclass"] = *aclass;
        if (type)   p["type"]   = *type;
        if (start)  p["start"]  = std::to_string(*start);
        if (end)    p["end"]    = std::to_string(*end);
        if (ofs)    p["ofs"]    = std::to_string(*ofs);
        return make_private_request("/0/private/Ledgers", p, creds);
    }
};

struct LedgersResult {
    std::map<std::string, exchange::kraken::LedgerEntry> ledger;
    int32_t count{0};
    static LedgersResult from_json(const json& j) {
        LedgersResult r;
        if (j.contains("ledger"))
            for (const auto& [k, v] : j["ledger"].items())
                r.ledger[k] = exchange::kraken::LedgerEntry::from_json(v, k);
        if (j.contains("count")) r.count = j["count"].get<int32_t>();
        return r;
    }
};

struct QueryLedgersRequest : TypedPrivateRequest<QueryLedgersResultWrapper> {
    std::vector<std::string> ids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        std::string id_str;
        for (size_t i = 0; i < ids.size(); ++i) { if (i) id_str += ','; id_str += ids[i]; }
        p["id"] = id_str;
        if (trades && *trades) p["trades"] = "true";
        return make_private_request("/0/private/QueryLedgers", p, creds);
    }
};

struct QueryLedgersResultWrapper {
    std::map<std::string, exchange::kraken::LedgerEntry> ledger;
    static QueryLedgersResultWrapper from_json(const json& j) {
        QueryLedgersResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.ledger[k] = exchange::kraken::LedgerEntry::from_json(v, k);
        return r;
    }
};

// ── TRADING (private) ─────────────────────────────────────────────────────────

// Helper: serialize an OrderParams into the REST param map.
inline void apply_order_params_to_rest(std::map<std::string, std::string>& p,
                                       const exchange::kraken::OrderParams& op) {
    p["ordertype"] = exchange::kraken::kraken_order_type_to_string(op.order_type);
    p["type"]      = exchange::kraken::to_string(op.side);
    p["volume"]    = std::to_string(op.order_qty);
    p["pair"]      = op.symbol;

    if (op.limit_price)   p["price"]      = op.limit_price->str();
    if (op.time_in_force) p["timeinforce"] = exchange::kraken::kraken_tif_to_string(*op.time_in_force);
    if (op.margin && *op.margin) p["leverage"] = "5";
    if (op.post_only && *op.post_only) p["oflags"] = "post";
    if (op.expire_time)   p["expiretm"]   = *op.expire_time;
    if (op.cl_ord_id)     p["cl_ord_id"]  = *op.cl_ord_id;
    if (op.order_userref) p["userref"]    = std::to_string(*op.order_userref);
    if (op.display_qty)   p["displayvol"] = std::to_string(*op.display_qty);
    if (op.validate && *op.validate) p["validate"] = "true";
    if (op.deadline)      p["deadline"]   = *op.deadline;

    if (op.triggers) p["price"] = op.triggers->price.str();

    if (op.conditional) {
        if (op.conditional->order_type)    p["close[ordertype]"] = exchange::kraken::kraken_order_type_to_string(*op.conditional->order_type);
        if (op.conditional->limit_price)   p["close[price]"]     = op.conditional->limit_price->str();
        if (op.conditional->trigger_price) p["close[price2]"]    = op.conditional->trigger_price->str();
    }
}

struct AddOrderRequest : TypedPrivateRequest<AddOrderResult> {
    exchange::kraken::OrderParams params;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        apply_order_params_to_rest(p, params);
        return make_private_request("/0/private/AddOrder", p, creds);
    }
};

struct AddOrderResult {
    std::string              descr_order;
    std::optional<std::string> descr_close;
    std::vector<std::string> txids;

    static AddOrderResult from_json(const json& j) {
        AddOrderResult r;
        if (j.contains("descr")) {
            r.descr_order = j["descr"].value("order", "");
            if (j["descr"].contains("close")) r.descr_close = j["descr"]["close"].get<std::string>();
        }
        if (j.contains("txid"))
            r.txids = j["txid"].get<std::vector<std::string>>();
        return r;
    }
};

struct AddOrderBatchRequest : TypedPrivateRequest<AddOrderBatchResult> {
    std::string pair;
    std::vector<exchange::kraken::OrderParams> orders;
    std::optional<bool>   validate;
    std::optional<std::string> deadline;

    HttpRequest build(const Credentials& creds) const override {
        json body = json::array();
        for (const auto& op : orders) {
            json o;
            o["ordertype"] = exchange::kraken::kraken_order_type_to_string(op.order_type);
            o["type"]      = exchange::kraken::to_string(op.side);
            o["volume"]    = std::to_string(op.order_qty);
            if (op.limit_price)   o["price"]     = op.limit_price->str();
            if (op.cl_ord_id)     o["cl_ord_id"] = *op.cl_ord_id;
            if (op.order_userref) o["userref"]   = *op.order_userref;
            body.push_back(o);
        }
        uint64_t n = make_nonce();
        std::string nonce_str = std::to_string(n);

        json req_body;
        req_body["nonce"]  = nonce_str;
        req_body["pair"]   = pair;
        req_body["orders"] = body;
        if (validate) req_body["validate"] = *validate;
        if (deadline) req_body["deadline"] = *deadline;

        std::string body_str = req_body.dump();
        std::string sign     = creds.sign("/0/private/AddOrderBatch", nonce_str, body_str);

        HttpRequest r;
        r.method = HttpRequest::Method::POST;
        r.path   = "/0/private/AddOrderBatch";
        r.body   = body_str;
        r.headers["Content-Type"] = "application/json";
        r.headers["API-Key"]      = creds.api_key;
        r.headers["API-Sign"]     = sign;
        return r;
    }
};

struct BatchOrderResult {
    std::string              descr_order;
    std::vector<std::string> txids;
    std::optional<std::string> error;
};

struct AddOrderBatchResult {
    std::vector<BatchOrderResult> orders;
    static AddOrderBatchResult from_json(const json& j) {
        AddOrderBatchResult r;
        if (j.contains("orders")) {
            for (const auto& o : j["orders"]) {
                BatchOrderResult br;
                if (o.contains("descr")) br.descr_order = o["descr"].value("order", "");
                if (o.contains("txid"))  br.txids        = o["txid"].get<std::vector<std::string>>();
                if (o.contains("error")) br.error        = o["error"].get<std::string>();
                r.orders.push_back(br);
            }
        }
        return r;
    }
};

struct EditOrderRequest : TypedPrivateRequest<EditOrderResult> {
    std::string txid;
    std::string pair;
    std::optional<double>      volume;
    std::optional<double>      price;
    std::optional<double>      price2;
    std::optional<double>      display_vol;
    std::optional<bool>        post_only;
    std::optional<std::string> deadline;
    std::optional<int64_t>     userref;
    std::optional<std::string> cl_ord_id;
    std::optional<bool>        validate;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        p["txid"] = txid;
        p["pair"] = pair;
        if (volume)      p["volume"]     = std::to_string(*volume);
        if (price)       p["price"]      = std::to_string(*price);
        if (price2)      p["price2"]     = std::to_string(*price2);
        if (display_vol) p["displayvol"] = std::to_string(*display_vol);
        if (post_only && *post_only) p["oflags"] = "post";
        if (deadline)    p["deadline"]   = *deadline;
        if (userref)     p["userref"]    = std::to_string(*userref);
        if (cl_ord_id)   p["cl_ord_id"]  = *cl_ord_id;
        if (validate && *validate) p["validate"] = "true";
        return make_private_request("/0/private/EditOrder", p, creds);
    }
};

struct EditOrderResult {
    std::string              descr_order;
    std::vector<std::string> txids;
    std::optional<std::string> orig_txid;

    static EditOrderResult from_json(const json& j) {
        EditOrderResult r;
        if (j.contains("descr")) r.descr_order = j["descr"].value("order", "");
        if (j.contains("txid"))  r.txids        = j["txid"].get<std::vector<std::string>>();
        if (j.contains("originaltxid")) r.orig_txid = j["originaltxid"].get<std::string>();
        return r;
    }
};

struct AmendOrderRequest : TypedPrivateRequest<AmendOrderResult> {
    std::optional<std::string> txid;
    std::optional<std::string> cl_ord_id;
    std::optional<double>      order_qty;
    std::optional<double>      display_qty;
    std::optional<double>      limit_price;
    std::optional<double>      trigger_price;
    std::optional<std::string> deadline;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (txid)          p["txid"]          = *txid;
        if (cl_ord_id)     p["cl_ord_id"]     = *cl_ord_id;
        if (order_qty)     p["order_qty"]     = std::to_string(*order_qty);
        if (display_qty)   p["display_qty"]   = std::to_string(*display_qty);
        if (limit_price)   p["limit_price"]   = std::to_string(*limit_price);
        if (trigger_price) p["trigger_price"] = std::to_string(*trigger_price);
        if (deadline)      p["deadline"]      = *deadline;
        return make_private_request("/0/private/AmendOrder", p, creds);
    }
};

struct AmendOrderResult {
    std::string amend_id;
    static AmendOrderResult from_json(const json& j) {
        return { j.value("amend_id", "") };
    }
};

struct CancelOrderRequest : TypedPrivateRequest<CancelOrderResult> {
    std::string txid;

    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CancelOrder", {{"txid", txid}}, creds);
    }
};

struct CancelOrderResult {
    int32_t count{0};
    bool    pending{false};
    static CancelOrderResult from_json(const json& j) {
        return { j.value("count", 0), j.value("pending", false) };
    }
};

struct CancelAllOrdersRequest : TypedPrivateRequest<CancelAllResult> {
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CancelAll", {}, creds);
    }
};

struct CancelAllResult {
    int32_t count{0};
    static CancelAllResult from_json(const json& j) {
        return { j.value("count", 0) };
    }
};

struct CancelAllOrdersAfterRequest : TypedPrivateRequest<CancelAllAfterResult> {
    int32_t timeout{0};

    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CancelAllOrdersAfter",
                                    {{"timeout", std::to_string(timeout)}}, creds);
    }
};

struct CancelAllAfterResult {
    std::string current_time;
    std::string trigger_time;
    static CancelAllAfterResult from_json(const json& j) {
        return { j.value("currentTime", ""), j.value("triggerTime", "") };
    }
};

struct CancelOrderBatchRequest : TypedPrivateRequest<CancelOrderBatchResult> {
    std::vector<std::string> orders;

    HttpRequest build(const Credentials& creds) const override {
        uint64_t n = make_nonce();
        std::string nonce_str = std::to_string(n);
        json req;
        req["nonce"]  = nonce_str;
        req["orders"] = orders;
        std::string body_str = req.dump();
        std::string sign     = creds.sign("/0/private/CancelOrderBatch", nonce_str, body_str);

        HttpRequest r;
        r.method = HttpRequest::Method::POST;
        r.path   = "/0/private/CancelOrderBatch";
        r.body   = body_str;
        r.headers["Content-Type"] = "application/json";
        r.headers["API-Key"]      = creds.api_key;
        r.headers["API-Sign"]     = sign;
        return r;
    }
};

struct CancelOrderBatchResult {
    int32_t count{0};
    static CancelOrderBatchResult from_json(const json& j) {
        return { j.value("count", 0) };
    }
};

struct GetWebSocketsTokenRequest : TypedPrivateRequest<WebSocketsTokenResult> {
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/GetWebSocketsToken", {}, creds);
    }
};

struct WebSocketsTokenResult {
    std::string token;
    int64_t     expires{0};
    static WebSocketsTokenResult from_json(const json& j) {
        return { j.value("token", ""), j.value("expires", int64_t{0}) };
    }
};

// ── FUNDING (private) ─────────────────────────────────────────────────────────

struct GetDepositMethodsRequest : TypedPrivateRequest<DepositMethodsResult> {
    std::string asset;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/DepositMethods", {{"asset", asset}}, creds);
    }
};

struct DepositMethod {
    std::string method;
    std::string limit;
    std::string fee;
    bool        gen_address{false};
    static DepositMethod from_json(const json& j) {
        return { j.value("method",""), j.value("limit",""), j.value("fee",""), j.value("gen-address", false) };
    }
};

struct DepositMethodsResult {
    std::vector<DepositMethod> methods;
    static DepositMethodsResult from_json(const json& j) {
        DepositMethodsResult r;
        for (const auto& m : j) r.methods.push_back(DepositMethod::from_json(m));
        return r;
    }
};

struct GetDepositAddressesRequest : TypedPrivateRequest<DepositAddressesResult> {
    std::string asset;
    std::string method;
    std::optional<bool> new_address;
    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p = {{"asset", asset}, {"method", method}};
        if (new_address && *new_address) p["new"] = "true";
        return make_private_request("/0/private/DepositAddresses", p, creds);
    }
};

struct DepositAddress {
    std::string address;
    std::string expiretm;
    bool        new_addr{false};
    static DepositAddress from_json(const json& j) {
        return { j.value("address",""), j.value("expiretm",""), j.value("new", false) };
    }
};

struct DepositAddressesResult {
    std::vector<DepositAddress> addresses;
    static DepositAddressesResult from_json(const json& j) {
        DepositAddressesResult r;
        for (const auto& a : j) r.addresses.push_back(DepositAddress::from_json(a));
        return r;
    }
};

struct WithdrawRequest : TypedPrivateRequest<WithdrawResult> {
    std::string asset;
    std::string key;
    std::string amount;
    std::optional<std::string> address;
    std::optional<std::string> max_fee;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p = {{"asset", asset}, {"key", key}, {"amount", amount}};
        if (address) p["address"] = *address;
        if (max_fee) p["max_fee"] = *max_fee;
        return make_private_request("/0/private/Withdraw", p, creds);
    }
};

struct WithdrawResult {
    std::string refid;
    static WithdrawResult from_json(const json& j) { return { j.value("refid", "") }; }
};

struct CancelWithdrawalRequest : TypedPrivateRequest<CancelWithdrawalResult> {
    std::string asset;
    std::string refid;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/WithdrawCancel",
                                    {{"asset", asset}, {"refid", refid}}, creds);
    }
};

struct CancelWithdrawalResult {
    bool result{false};
    static CancelWithdrawalResult from_json(const json& j) { return { j.get<bool>() }; }
};

// ── SUBACCOUNTS (private) ─────────────────────────────────────────────────────

struct CreateSubaccountRequest : TypedPrivateRequest<CreateSubaccountResult> {
    std::string username;
    std::string email;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CreateSubaccount",
                                    {{"username", username}, {"email", email}}, creds);
    }
};

struct CreateSubaccountResult {
    bool result{false};
    static CreateSubaccountResult from_json(const json& j) { return { j.get<bool>() }; }
};

// ── EARN (private) ────────────────────────────────────────────────────────────

struct AllocateEarnRequest : TypedPrivateRequest<EarnBoolResult> {
    std::string strategy_id;
    std::string amount;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/Earn/Allocate",
                                    {{"strategy_id", strategy_id}, {"amount", amount}}, creds);
    }
};

struct DeallocateEarnRequest : TypedPrivateRequest<EarnBoolResult> {
    std::string strategy_id;
    std::string amount;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/Earn/Deallocate",
                                    {{"strategy_id", strategy_id}, {"amount", amount}}, creds);
    }
};

struct EarnBoolResult {
    bool result{false};
    static EarnBoolResult from_json(const json& j) { return { j.get<bool>() }; }
};

} // namespace exchange::kraken::rest
