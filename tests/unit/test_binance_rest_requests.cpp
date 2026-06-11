// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/rest_api.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace exchange::binance::rest;

// ---------------------------------------------------------------------------
// Public requests — verify path, method, and query parameters.
// ---------------------------------------------------------------------------

TEST(BinancePublicRequests, Ping_Path) {
    BinancePingRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ping");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, ServerTime_Path) {
    BinanceServerTimeRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/time");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, TickerPrice_NoParams) {
    BinanceTickerPriceRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ticker/price");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, TickerPrice_SingleSymbol) {
    BinanceTickerPriceRequest req;
    req.symbol = "BTCUSDT";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ticker/price");
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, TickerPrice_MultipleSymbols) {
    BinanceTickerPriceRequest req;
    req.symbols = {"BTCUSDT", "ETHBTC"};
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ticker/price");
    EXPECT_EQ(http.query, "symbols=%5B%22BTCUSDT%22%2C%22ETHBTC%22%5D");
}

TEST(BinancePublicRequests, TickerPrice_SymbolPreferredOverSymbols) {
    BinanceTickerPriceRequest req;
    req.symbol  = "BTCUSDT";
    req.symbols = {"ETHBTC"};
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, OrderBook_SymbolOnly) {
    BinanceOrderBookRequest req;
    req.symbol = "BTCUSDT";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/depth");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, OrderBook_WithLimit) {
    BinanceOrderBookRequest req;
    req.symbol = "BTCUSDT";
    req.limit  = 50;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&limit=50");
}

TEST(BinancePublicRequests, RecentTrades_SymbolOnly) {
    BinanceRecentTradesRequest req;
    req.symbol = "BTCUSDT";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/trades");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
}

TEST(BinancePublicRequests, RecentTrades_WithLimit) {
    BinanceRecentTradesRequest req;
    req.symbol = "BTCUSDT";
    req.limit  = 10;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&limit=10");
}

TEST(BinancePublicRequests, Klines_SymbolAndInterval) {
    BinanceKlinesRequest req;
    req.symbol   = "BTCUSDT";
    req.interval = "1m";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/klines");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m");
}

TEST(BinancePublicRequests, Klines_AllOptionals_InOrder) {
    BinanceKlinesRequest req;
    req.symbol     = "BTCUSDT";
    req.interval   = "1h";
    req.start_time = 1499040000000LL;
    req.end_time   = 1499644799999LL;
    req.limit      = 500;
    auto http = req.build();
    EXPECT_EQ(http.query,
              "symbol=BTCUSDT&interval=1h"
              "&startTime=1499040000000&endTime=1499644799999&limit=500");
}

TEST(BinancePublicRequests, Klines_OnlyStartTime) {
    BinanceKlinesRequest req;
    req.symbol     = "BTCUSDT";
    req.interval   = "1m";
    req.start_time = 1499040000000LL;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m&startTime=1499040000000");
}

TEST(BinancePublicRequests, Klines_OnlyEndTime) {
    BinanceKlinesRequest req;
    req.symbol   = "BTCUSDT";
    req.interval = "1m";
    req.end_time = 1499644799999LL;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m&endTime=1499644799999");
}

TEST(BinancePublicRequests, Klines_OnlyLimit) {
    BinanceKlinesRequest req;
    req.symbol   = "BTCUSDT";
    req.interval = "1m";
    req.limit    = 100;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BTCUSDT&interval=1m&limit=100");
}

TEST(BinancePublicRequests, ExchangeInfo_Path) {
    BinanceExchangeInfoRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/exchangeInfo");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, Ticker24hr_NoParams) {
    BinanceTicker24hrRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/ticker/24hr");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePublicRequests, Ticker24hr_SingleSymbol) {
    BinanceTicker24hrRequest req;
    req.symbol = "BNBBTC";
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BNBBTC");
}

TEST(BinancePublicRequests, Ticker24hr_MultipleSymbols) {
    BinanceTicker24hrRequest req;
    req.symbols = {"BTCUSDT", "ETHBTC"};
    auto http = req.build();
    EXPECT_EQ(http.query, "symbols=%5B%22BTCUSDT%22%2C%22ETHBTC%22%5D");
}

// ---------------------------------------------------------------------------
// Private (signed) requests — build() constructs the request WITHOUT auth
// params; BinanceAuth::sign() appends timestamp/recvWindow/signature later.
// ---------------------------------------------------------------------------

TEST(BinancePrivateRequests, Account_Path) {
    BinanceAccountRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/account");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
    EXPECT_TRUE(http.body.empty());
}

TEST(BinancePrivateRequests, OpenOrders_NoSymbol) {
    BinanceOpenOrdersRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/openOrders");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(BinancePrivateRequests, OpenOrders_WithSymbol) {
    BinanceOpenOrdersRequest req;
    req.symbol = "LTCBTC";
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=LTCBTC");
}

TEST(BinancePrivateRequests, AllOrders_RequiredOnly) {
    BinanceAllOrdersRequest req;
    req.symbol = "LTCBTC";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/allOrders");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=LTCBTC");
    EXPECT_TRUE(http.body.empty());
}

TEST(BinancePrivateRequests, AllOrders_AllOptionals) {
    BinanceAllOrdersRequest req;
    req.symbol     = "LTCBTC";
    req.order_id   = 1;
    req.start_time = 1499827319559LL;
    req.end_time   = 1499827319560LL;
    req.limit      = 10;
    auto http = req.build();
    EXPECT_EQ(http.query,
              "symbol=LTCBTC&orderId=1&startTime=1499827319559"
              "&endTime=1499827319560&limit=10");
}

TEST(BinancePrivateRequests, MyTrades_RequiredOnly) {
    BinanceMyTradesRequest req;
    req.symbol = "BNBBTC";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/myTrades");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_EQ(http.query, "symbol=BNBBTC");
}

TEST(BinancePrivateRequests, MyTrades_FromIdAndLimit) {
    BinanceMyTradesRequest req;
    req.symbol  = "BNBBTC";
    req.from_id = 28000;
    req.limit   = 5;
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=BNBBTC&fromId=28000&limit=5");
}

// ---------------------------------------------------------------------------
// Trading requests (Step 6.4) — POST params live in the body (query empty);
// DELETE params live in the query (body empty). BinanceAuth signs whichever
// side the method dictates, so a param on the wrong side would corrupt the
// signed payload.
// ---------------------------------------------------------------------------

TEST(BinanceTradingRequests, NewOrder_MarketMinimal) {
    BinanceNewOrderRequest req;
    req.symbol   = "BTCUSDT";
    req.side     = exchange::Side::Sell;
    req.type     = exchange::OrderType::Market;
    req.quantity = "10.00000000";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/order");
    EXPECT_EQ(http.method, HttpRequest::Method::POST);
    EXPECT_TRUE(http.query.empty());
    EXPECT_EQ(http.body, "symbol=BTCUSDT&side=SELL&type=MARKET&quantity=10.00000000");
}

TEST(BinanceTradingRequests, NewOrder_LimitAllOptions) {
    BinanceNewOrderRequest req;
    req.symbol              = "BTCUSDT";
    req.side                = exchange::Side::Buy;
    req.type                = exchange::OrderType::Limit;
    req.time_in_force       = exchange::TimeInForce::GTC;
    req.quantity            = "1.00000000";
    req.quote_order_qty     = "50000.00000000";
    req.price               = "40000.00000000";
    req.new_client_order_id = "myOrder1";
    req.stop_price          = "39000.00000000";
    req.iceberg_qty         = "0.10000000";
    req.new_order_resp_type = exchange::binance::BinanceOrderRespType::Full;
    auto http = req.build();
    EXPECT_TRUE(http.query.empty());
    EXPECT_EQ(http.body,
              "symbol=BTCUSDT&side=BUY&type=LIMIT&timeInForce=GTC"
              "&quantity=1.00000000&quoteOrderQty=50000.00000000"
              "&price=40000.00000000&newClientOrderId=myOrder1"
              "&stopPrice=39000.00000000&icebergQty=0.10000000"
              "&newOrderRespType=FULL");
}

TEST(BinanceTradingRequests, NewOrder_UnsupportedTypeThrows) {
    // TrailingStop has no Binance Spot wire format — build() must propagate
    // binance_order_type_to_string's throw rather than emit a bogus param.
    BinanceNewOrderRequest req;
    req.symbol = "BTCUSDT";
    req.type   = exchange::OrderType::TrailingStop;
    EXPECT_THROW(req.build(), std::invalid_argument);
}

TEST(BinanceTradingRequests, CancelOrder_ByOrderId) {
    BinanceCancelOrderRequest req;
    req.symbol   = "LTCBTC";
    req.order_id = 4;
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/order");
    EXPECT_EQ(http.method, HttpRequest::Method::DELETE);
    EXPECT_EQ(http.query, "symbol=LTCBTC&orderId=4");
    EXPECT_TRUE(http.body.empty());
}

TEST(BinanceTradingRequests, CancelOrder_ByOrigClientOrderId) {
    BinanceCancelOrderRequest req;
    req.symbol               = "LTCBTC";
    req.orig_client_order_id = "myOrder1";
    auto http = req.build();
    EXPECT_EQ(http.query, "symbol=LTCBTC&origClientOrderId=myOrder1");
}

TEST(BinanceTradingRequests, CancelAllOpenOrders_Query) {
    BinanceCancelAllOpenOrdersRequest req;
    req.symbol = "BTCUSDT";
    auto http = req.build();
    EXPECT_EQ(http.path, "/api/v3/openOrders");
    EXPECT_EQ(http.method, HttpRequest::Method::DELETE);
    EXPECT_EQ(http.query, "symbol=BTCUSDT");
    EXPECT_TRUE(http.body.empty());
}
