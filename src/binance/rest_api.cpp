// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/rest_api.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace exchange::binance::rest {

// ── Helpers ────────────────────────────────────────────────────────────────────

namespace detail {

std::string symbols_query_value(const std::vector<std::string>& symbols) {
    std::string out = "%5B";
    for (size_t i = 0; i < symbols.size(); ++i) {
        if (i) out += "%2C";
        out += "%22";
        out += symbols[i];
        out += "%22";
    }
    out += "%5D";
    return out;
}

} // namespace detail

// ── GET /api/v3/ping ─────────────────────────────────────────────────────────

BinancePing BinancePing::from_json(const json&) { return {}; }

HttpRequest BinancePingRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/ping";
    return r;
}

// ── GET /api/v3/time ─────────────────────────────────────────────────────────

BinanceServerTime BinanceServerTime::from_json(const json& j) {
    BinanceServerTime t;
    t.server_time = j.value("serverTime", int64_t{0});
    return t;
}

HttpRequest BinanceServerTimeRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/time";
    return r;
}

// ── GET /api/v3/ticker/price ─────────────────────────────────────────────────

BinanceTickerPriceEntry BinanceTickerPriceEntry::from_json(const json& j) {
    BinanceTickerPriceEntry e;
    e.symbol = j.value("symbol", "");
    e.price  = std::stod(j.value("price", "0"));
    return e;
}

BinanceTickerPrice BinanceTickerPrice::from_json(const json& j) {
    BinanceTickerPrice t;
    if (j.is_array()) {
        t.entries.reserve(j.size());
        for (const auto& el : j)
            t.entries.push_back(BinanceTickerPriceEntry::from_json(el));
    } else {
        t.entries.push_back(BinanceTickerPriceEntry::from_json(j));
    }
    return t;
}

HttpRequest BinanceTickerPriceRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/ticker/price";
    if (symbol) {
        r.query = "symbol=" + *symbol;
    } else if (symbols && !symbols->empty()) {
        r.query = "symbols=" + detail::symbols_query_value(*symbols);
    }
    return r;
}

// ── GET /api/v3/depth ────────────────────────────────────────────────────────

BinanceOrderBook BinanceOrderBook::from_json(const json& j) {
    BinanceOrderBook b;
    b.last_update_id = j.value("lastUpdateId", int64_t{0});
    if (j.contains("bids")) {
        b.bids.reserve(j["bids"].size());
        for (const auto& row : j["bids"])
            b.bids.push_back(BinanceBookLevel::from_json(row));
    }
    if (j.contains("asks")) {
        b.asks.reserve(j["asks"].size());
        for (const auto& row : j["asks"])
            b.asks.push_back(BinanceBookLevel::from_json(row));
    }
    return b;
}

HttpRequest BinanceOrderBookRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/depth";
    r.query  = "symbol=" + symbol;
    if (limit) r.query += "&limit=" + std::to_string(*limit);
    return r;
}

// ── GET /api/v3/trades ───────────────────────────────────────────────────────

BinanceTrade BinanceTrade::from_json(const json& j) {
    BinanceTrade t;
    t.id             = j.value("id", int64_t{0});
    t.price          = std::stod(j.value("price", "0"));
    t.qty            = std::stod(j.value("qty", "0"));
    t.quote_qty      = std::stod(j.value("quoteQty", "0"));
    t.time           = j.value("time", int64_t{0});
    t.is_buyer_maker = j.value("isBuyerMaker", false);
    t.is_best_match  = j.value("isBestMatch", false);
    return t;
}

BinanceTradesResult BinanceTradesResult::from_json(const json& j) {
    BinanceTradesResult r;
    r.trades.reserve(j.size());
    for (const auto& el : j)
        r.trades.push_back(BinanceTrade::from_json(el));
    return r;
}

HttpRequest BinanceRecentTradesRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/trades";
    r.query  = "symbol=" + symbol;
    if (limit) r.query += "&limit=" + std::to_string(*limit);
    return r;
}

// ── GET /api/v3/klines ───────────────────────────────────────────────────────

BinanceKline BinanceKline::from_json(const json& row) {
    BinanceKline k;
    k.open_time              = row.at(0).get<int64_t>();
    k.open                   = std::stod(row.at(1).get<std::string>());
    k.high                   = std::stod(row.at(2).get<std::string>());
    k.low                    = std::stod(row.at(3).get<std::string>());
    k.close                  = std::stod(row.at(4).get<std::string>());
    k.volume                 = std::stod(row.at(5).get<std::string>());
    k.close_time             = row.at(6).get<int64_t>();
    k.quote_asset_volume     = std::stod(row.at(7).get<std::string>());
    k.num_trades             = row.at(8).get<int64_t>();
    k.taker_buy_base_volume  = std::stod(row.at(9).get<std::string>());
    k.taker_buy_quote_volume = std::stod(row.at(10).get<std::string>());
    return k;
}

BinanceKlinesResult BinanceKlinesResult::from_json(const json& j) {
    BinanceKlinesResult r;
    r.klines.reserve(j.size());
    for (const auto& row : j)
        r.klines.push_back(BinanceKline::from_json(row));
    return r;
}

HttpRequest BinanceKlinesRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/klines";
    r.query  = "symbol=" + symbol + "&interval=" + interval;
    if (start_time) r.query += "&startTime=" + std::to_string(*start_time);
    if (end_time)   r.query += "&endTime=" + std::to_string(*end_time);
    if (limit)      r.query += "&limit=" + std::to_string(*limit);
    return r;
}

// ── GET /api/v3/exchangeInfo ─────────────────────────────────────────────────

BinanceSymbolInfo BinanceSymbolInfo::from_json(const json& j) {
    BinanceSymbolInfo s;
    s.symbol                    = j.value("symbol", "");
    s.status                    = j.value("status", "");
    s.base_asset                = j.value("baseAsset", "");
    s.base_asset_precision      = j.value("baseAssetPrecision", 0);
    s.quote_asset               = j.value("quoteAsset", "");
    s.quote_precision           = j.value("quotePrecision", 0);
    s.quote_asset_precision     = j.value("quoteAssetPrecision", 0);
    if (j.contains("orderTypes")) {
        s.order_types.reserve(j["orderTypes"].size());
        for (const auto& ot : j["orderTypes"])
            s.order_types.push_back(ot.get<std::string>());
    }
    s.iceberg_allowed           = j.value("icebergAllowed", false);
    s.oco_allowed               = j.value("ocoAllowed", false);
    s.is_spot_trading_allowed   = j.value("isSpotTradingAllowed", false);
    s.is_margin_trading_allowed = j.value("isMarginTradingAllowed", false);
    return s;
}

BinanceExchangeInfo BinanceExchangeInfo::from_json(const json& j) {
    BinanceExchangeInfo e;
    e.timezone    = j.value("timezone", "");
    e.server_time = j.value("serverTime", int64_t{0});
    if (j.contains("symbols")) {
        e.symbols.reserve(j["symbols"].size());
        for (const auto& s : j["symbols"])
            e.symbols.push_back(BinanceSymbolInfo::from_json(s));
    }
    return e;
}

HttpRequest BinanceExchangeInfoRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/exchangeInfo";
    return r;
}

// ── GET /api/v3/ticker/24hr ──────────────────────────────────────────────────

BinanceTicker24hrEntry BinanceTicker24hrEntry::from_json(const json& j) {
    BinanceTicker24hrEntry e;
    e.symbol               = j.value("symbol", "");
    e.price_change         = std::stod(j.value("priceChange", "0"));
    e.price_change_percent = std::stod(j.value("priceChangePercent", "0"));
    e.weighted_avg_price   = std::stod(j.value("weightedAvgPrice", "0"));
    e.prev_close_price     = std::stod(j.value("prevClosePrice", "0"));
    e.last_price           = std::stod(j.value("lastPrice", "0"));
    e.last_qty             = std::stod(j.value("lastQty", "0"));
    e.bid_price            = std::stod(j.value("bidPrice", "0"));
    e.bid_qty              = std::stod(j.value("bidQty", "0"));
    e.ask_price            = std::stod(j.value("askPrice", "0"));
    e.ask_qty              = std::stod(j.value("askQty", "0"));
    e.open_price           = std::stod(j.value("openPrice", "0"));
    e.high_price           = std::stod(j.value("highPrice", "0"));
    e.low_price            = std::stod(j.value("lowPrice", "0"));
    e.volume               = std::stod(j.value("volume", "0"));
    e.quote_volume         = std::stod(j.value("quoteVolume", "0"));
    e.open_time            = j.value("openTime", int64_t{0});
    e.close_time           = j.value("closeTime", int64_t{0});
    e.first_id             = j.value("firstId", int64_t{0});
    e.last_id              = j.value("lastId", int64_t{0});
    e.count                = j.value("count", int64_t{0});
    return e;
}

BinanceTicker24hr BinanceTicker24hr::from_json(const json& j) {
    BinanceTicker24hr t;
    if (j.is_array()) {
        t.entries.reserve(j.size());
        for (const auto& el : j)
            t.entries.push_back(BinanceTicker24hrEntry::from_json(el));
    } else {
        t.entries.push_back(BinanceTicker24hrEntry::from_json(j));
    }
    return t;
}

HttpRequest BinanceTicker24hrRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/ticker/24hr";
    if (symbol) {
        r.query = "symbol=" + *symbol;
    } else if (symbols && !symbols->empty()) {
        r.query = "symbols=" + detail::symbols_query_value(*symbols);
    }
    return r;
}

// ── GET /api/v3/account ──────────────────────────────────────────────────────

BinanceCommissionRates BinanceCommissionRates::from_json(const json& j) {
    BinanceCommissionRates r;
    r.maker  = std::stod(j.value("maker", "0"));
    r.taker  = std::stod(j.value("taker", "0"));
    r.buyer  = std::stod(j.value("buyer", "0"));
    r.seller = std::stod(j.value("seller", "0"));
    return r;
}

BinanceBalance BinanceBalance::from_json(const json& j) {
    BinanceBalance b;
    b.asset  = j.value("asset", "");
    b.free   = std::stod(j.value("free", "0"));
    b.locked = std::stod(j.value("locked", "0"));
    return b;
}

BinanceAccount BinanceAccount::from_json(const json& j) {
    BinanceAccount a;
    a.maker_commission  = j.value("makerCommission", 0);
    a.taker_commission  = j.value("takerCommission", 0);
    a.buyer_commission  = j.value("buyerCommission", 0);
    a.seller_commission = j.value("sellerCommission", 0);
    if (j.contains("commissionRates"))
        a.commission_rates = BinanceCommissionRates::from_json(j.at("commissionRates"));
    a.can_trade                     = j.value("canTrade", false);
    a.can_withdraw                  = j.value("canWithdraw", false);
    a.can_deposit                   = j.value("canDeposit", false);
    a.brokered                      = j.value("brokered", false);
    a.require_self_trade_prevention = j.value("requireSelfTradePrevention", false);
    a.prevent_sor                   = j.value("preventSor", false);
    a.update_time                   = j.value("updateTime", int64_t{0});
    a.account_type                  = j.value("accountType", "");
    const json balances = j.value("balances", json::array());
    a.balances.reserve(balances.size());
    for (const auto& el : balances)
        a.balances.push_back(BinanceBalance::from_json(el));
    const json permissions = j.value("permissions", json::array());
    a.permissions.reserve(permissions.size());
    for (const auto& el : permissions)
        a.permissions.push_back(el.get<std::string>());
    a.uid = j.value("uid", int64_t{0});
    return a;
}

HttpRequest BinanceAccountRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/account";
    return r;
}

// ── GET /api/v3/openOrders and /api/v3/allOrders ─────────────────────────────

BinanceOrderInfo BinanceOrderInfo::from_json(const json& j) {
    BinanceOrderInfo o;
    o.symbol                = j.value("symbol", "");
    o.order_id              = j.value("orderId", int64_t{0});
    o.order_list_id         = j.value("orderListId", int64_t{-1});
    o.client_order_id       = j.value("clientOrderId", "");
    o.price                 = std::stod(j.value("price", "0"));
    o.orig_qty              = std::stod(j.value("origQty", "0"));
    o.executed_qty          = std::stod(j.value("executedQty", "0"));
    o.cummulative_quote_qty = std::stod(j.value("cummulativeQuoteQty", "0"));
    o.orig_quote_order_qty  = std::stod(j.value("origQuoteOrderQty", "0"));
    o.status                = binance_order_status_from_string(j.value("status", ""));
    o.time_in_force         = binance_tif_from_string(j.value("timeInForce", "GTC"));
    o.type                  = j.value("type", "");
    o.side                  = binance_side_from_string(j.value("side", "BUY"));
    o.stop_price            = std::stod(j.value("stopPrice", "0"));
    o.iceberg_qty           = std::stod(j.value("icebergQty", "0"));
    o.time                  = j.value("time", int64_t{0});
    o.update_time           = j.value("updateTime", int64_t{0});
    o.is_working            = j.value("isWorking", false);
    o.working_time          = j.value("workingTime", int64_t{0});
    o.self_trade_prevention_mode = j.value("selfTradePreventionMode", "");
    return o;
}

BinanceOpenOrdersResult BinanceOpenOrdersResult::from_json(const json& j) {
    BinanceOpenOrdersResult r;
    r.orders.reserve(j.size());
    for (const auto& el : j)
        r.orders.push_back(BinanceOrderInfo::from_json(el));
    return r;
}

HttpRequest BinanceOpenOrdersRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/openOrders";
    if (symbol) r.query = "symbol=" + *symbol;
    return r;
}

HttpRequest BinanceAllOrdersRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/allOrders";
    r.query  = "symbol=" + symbol;
    if (order_id)   r.query += "&orderId=" + std::to_string(*order_id);
    if (start_time) r.query += "&startTime=" + std::to_string(*start_time);
    if (end_time)   r.query += "&endTime=" + std::to_string(*end_time);
    if (limit)      r.query += "&limit=" + std::to_string(*limit);
    return r;
}

// ── GET /api/v3/myTrades ─────────────────────────────────────────────────────

BinanceMyTrade BinanceMyTrade::from_json(const json& j) {
    BinanceMyTrade t;
    t.symbol           = j.value("symbol", "");
    t.id               = j.value("id", int64_t{0});
    t.order_id         = j.value("orderId", int64_t{0});
    t.order_list_id    = j.value("orderListId", int64_t{-1});
    t.price            = std::stod(j.value("price", "0"));
    t.qty              = std::stod(j.value("qty", "0"));
    t.quote_qty        = std::stod(j.value("quoteQty", "0"));
    t.commission       = std::stod(j.value("commission", "0"));
    t.commission_asset = j.value("commissionAsset", "");
    t.time             = j.value("time", int64_t{0});
    t.is_buyer         = j.value("isBuyer", false);
    t.is_maker         = j.value("isMaker", false);
    t.is_best_match    = j.value("isBestMatch", false);
    return t;
}

BinanceMyTradesResult BinanceMyTradesResult::from_json(const json& j) {
    BinanceMyTradesResult r;
    r.trades.reserve(j.size());
    for (const auto& el : j)
        r.trades.push_back(BinanceMyTrade::from_json(el));
    return r;
}

HttpRequest BinanceMyTradesRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::GET;
    r.path   = "/api/v3/myTrades";
    r.query  = "symbol=" + symbol;
    if (order_id)   r.query += "&orderId=" + std::to_string(*order_id);
    if (start_time) r.query += "&startTime=" + std::to_string(*start_time);
    if (end_time)   r.query += "&endTime=" + std::to_string(*end_time);
    if (from_id)    r.query += "&fromId=" + std::to_string(*from_id);
    if (limit)      r.query += "&limit=" + std::to_string(*limit);
    return r;
}

// ── POST /api/v3/order ───────────────────────────────────────────────────────

BinanceFill BinanceFill::from_json(const json& j) {
    BinanceFill f;
    f.price            = std::stod(j.value("price", "0"));
    f.qty              = std::stod(j.value("qty", "0"));
    f.commission       = std::stod(j.value("commission", "0"));
    f.commission_asset = j.value("commissionAsset", "");
    f.trade_id         = j.value("tradeId", int64_t{0});
    return f;
}

BinanceNewOrderResponse BinanceNewOrderResponse::from_json(const json& j) {
    BinanceNewOrderResponse r;
    r.symbol          = j.value("symbol", "");
    r.order_id        = j.value("orderId", int64_t{0});
    r.order_list_id   = j.value("orderListId", int64_t{-1});
    r.client_order_id = j.value("clientOrderId", "");
    r.transact_time   = j.value("transactTime", int64_t{0});
    if (j.contains("price"))
        r.price = std::stod(j.at("price").get<std::string>());
    if (j.contains("origQty"))
        r.orig_qty = std::stod(j.at("origQty").get<std::string>());
    if (j.contains("executedQty"))
        r.executed_qty = std::stod(j.at("executedQty").get<std::string>());
    if (j.contains("origQuoteOrderQty"))
        r.orig_quote_order_qty = std::stod(j.at("origQuoteOrderQty").get<std::string>());
    if (j.contains("cummulativeQuoteQty"))
        r.cummulative_quote_qty = std::stod(j.at("cummulativeQuoteQty").get<std::string>());
    if (j.contains("status"))
        r.status = binance_order_status_from_string(j.at("status").get<std::string>());
    if (j.contains("timeInForce"))
        r.time_in_force = binance_tif_from_string(j.at("timeInForce").get<std::string>());
    if (j.contains("type"))
        r.type = j.at("type").get<std::string>();
    if (j.contains("side"))
        r.side = binance_side_from_string(j.at("side").get<std::string>());
    if (j.contains("workingTime"))
        r.working_time = j.at("workingTime").get<int64_t>();
    if (j.contains("selfTradePreventionMode"))
        r.self_trade_prevention_mode = j.at("selfTradePreventionMode").get<std::string>();
    const json fills = j.value("fills", json::array());
    r.fills.reserve(fills.size());
    for (const auto& el : fills)
        r.fills.push_back(BinanceFill::from_json(el));
    return r;
}

HttpRequest BinanceNewOrderRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::POST;
    r.path   = "/api/v3/order";
    r.body   = "symbol=" + symbol;
    r.body += "&side=" + exchange::binance::binance_side_to_string(side);
    r.body += "&type=" + exchange::binance::binance_order_type_to_string(type);
    if (time_in_force)
        r.body += "&timeInForce=" + exchange::binance::binance_tif_to_string(*time_in_force);
    if (quantity)            r.body += "&quantity=" + *quantity;
    if (quote_order_qty)     r.body += "&quoteOrderQty=" + *quote_order_qty;
    if (price)               r.body += "&price=" + *price;
    if (new_client_order_id) r.body += "&newClientOrderId=" + *new_client_order_id;
    if (stop_price)          r.body += "&stopPrice=" + *stop_price;
    if (iceberg_qty)         r.body += "&icebergQty=" + *iceberg_qty;
    if (new_order_resp_type)
        r.body += "&newOrderRespType="
                + exchange::binance::binance_order_resp_type_to_string(*new_order_resp_type);
    return r;
}

// ── DELETE /api/v3/order and /api/v3/openOrders ──────────────────────────────

BinanceCancelOrderResponse BinanceCancelOrderResponse::from_json(const json& j) {
    BinanceCancelOrderResponse r;
    r.symbol                = j.value("symbol", "");
    r.orig_client_order_id  = j.value("origClientOrderId", "");
    r.order_id              = j.value("orderId", int64_t{0});
    r.order_list_id         = j.value("orderListId", int64_t{-1});
    r.client_order_id       = j.value("clientOrderId", "");
    r.transact_time         = j.value("transactTime", int64_t{0});
    r.price                 = std::stod(j.value("price", "0"));
    r.orig_qty              = std::stod(j.value("origQty", "0"));
    r.executed_qty          = std::stod(j.value("executedQty", "0"));
    r.orig_quote_order_qty  = std::stod(j.value("origQuoteOrderQty", "0"));
    r.cummulative_quote_qty = std::stod(j.value("cummulativeQuoteQty", "0"));
    r.status                = binance_order_status_from_string(j.value("status", ""));
    r.time_in_force         = binance_tif_from_string(j.value("timeInForce", "GTC"));
    r.type                  = j.value("type", "");
    r.side                  = binance_side_from_string(j.value("side", "BUY"));
    r.self_trade_prevention_mode = j.value("selfTradePreventionMode", "");
    return r;
}

BinanceCancelAllResponse BinanceCancelAllResponse::from_json(const json& j) {
    BinanceCancelAllResponse r;
    r.orders.reserve(j.size());
    for (const auto& el : j)
        r.orders.push_back(BinanceCancelOrderResponse::from_json(el));
    return r;
}

HttpRequest BinanceCancelOrderRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::DELETE;
    r.path   = "/api/v3/order";
    r.query  = "symbol=" + symbol;
    if (order_id)
        r.query += "&orderId=" + std::to_string(*order_id);
    if (orig_client_order_id)
        r.query += "&origClientOrderId=" + *orig_client_order_id;
    if (new_client_order_id)
        r.query += "&newClientOrderId=" + *new_client_order_id;
    return r;
}

HttpRequest BinanceCancelAllOpenOrdersRequest::build() const {
    HttpRequest r;
    r.method = HttpRequest::Method::DELETE;
    r.path   = "/api/v3/openOrders";
    r.query  = "symbol=" + symbol;
    return r;
}

} // namespace exchange::binance::rest
