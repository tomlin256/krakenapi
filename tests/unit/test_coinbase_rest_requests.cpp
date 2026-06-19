// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Verifies that each Coinbase REST request builds the correct HTTP method,
// path, and query string. Response parsing is covered separately in
// test_coinbase_rest_responses.cpp.

#include "exchange/coinbase/rest_api.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace exchange::coinbase;        // Side, OrderType, TimeInForce
using namespace exchange::coinbase::rest;  // request types
using json = nlohmann::json;
using M = HttpRequest::Method;

// ── Public endpoints ──────────────────────────────────────────────────────────

TEST(CoinbaseRestRequests, ServerTime) {
    auto r = CoinbaseServerTimeRequest{}.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/time");
    EXPECT_TRUE(r.query.empty());
    EXPECT_TRUE(r.body.empty());
}

TEST(CoinbaseRestRequests, Products) {
    auto r = CoinbaseProductsRequest{}.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/products");
    EXPECT_TRUE(r.query.empty());
}

TEST(CoinbaseRestRequests, SingleProduct) {
    CoinbaseProductRequest req;
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/products/BTC-USD");
}

TEST(CoinbaseRestRequests, OrderBook_WithLevel) {
    CoinbaseOrderBookRequest req;
    req.product_id = "BTC-USD";
    req.level      = 2;
    auto r = req.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/products/BTC-USD/book");
    EXPECT_EQ(r.query, "level=2");
}

TEST(CoinbaseRestRequests, OrderBook_WithoutLevel_NoQuery) {
    CoinbaseOrderBookRequest req;
    req.product_id = "ETH-EUR";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/ETH-EUR/book");
    EXPECT_TRUE(r.query.empty());
}

TEST(CoinbaseRestRequests, Ticker) {
    CoinbaseTickerRequest req;
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/ticker");
    EXPECT_TRUE(r.query.empty());
}

TEST(CoinbaseRestRequests, Trades_WithLimit) {
    CoinbaseTradesRequest req;
    req.product_id = "BTC-USD";
    req.limit      = 50;
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/trades");
    EXPECT_EQ(r.query, "limit=50");
}

TEST(CoinbaseRestRequests, Candles_AllParams) {
    CoinbaseCandlesRequest req;
    req.product_id  = "BTC-USD";
    req.granularity = 60;
    req.start       = "2026-06-19T00:00:00Z";
    req.end         = "2026-06-19T01:00:00Z";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/candles");
    EXPECT_EQ(r.query,
              "granularity=60&start=2026-06-19T00:00:00Z&end=2026-06-19T01:00:00Z");
}

TEST(CoinbaseRestRequests, Candles_GranularityOnly) {
    CoinbaseCandlesRequest req;
    req.product_id  = "BTC-USD";
    req.granularity = 3600;
    auto r = req.build();
    EXPECT_EQ(r.query, "granularity=3600");
}

TEST(CoinbaseRestRequests, Stats) {
    CoinbaseStatsRequest req;
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.path, "/products/BTC-USD/stats");
}

// ── Private endpoints ─────────────────────────────────────────────────────────

TEST(CoinbaseRestRequests, Accounts) {
    auto r = CoinbaseAccountsRequest{}.build();
    EXPECT_EQ(r.method, M::GET);
    EXPECT_EQ(r.path, "/accounts");
}

TEST(CoinbaseRestRequests, SingleAccount) {
    CoinbaseAccountRequest req;
    req.account_id = "acc-1";
    EXPECT_EQ(req.build().path, "/accounts/acc-1");
}

TEST(CoinbaseRestRequests, PlaceLimitOrder_BuildsJsonBody) {
    CoinbasePlaceOrderRequest req;
    req.product_id    = "BTC-USD";
    req.side          = Side::Buy;
    req.type          = OrderType::Limit;
    req.price         = "30000.00";
    req.size          = "0.01";
    req.time_in_force = TimeInForce::GTC;
    req.post_only     = true;
    req.client_oid    = "abc-123";

    auto r = req.build();
    EXPECT_EQ(r.method, M::POST);
    EXPECT_EQ(r.path, "/orders");

    auto body = json::parse(r.body);
    EXPECT_EQ(body.at("product_id"),    "BTC-USD");
    EXPECT_EQ(body.at("side"),          "buy");
    EXPECT_EQ(body.at("type"),          "limit");
    EXPECT_EQ(body.at("price"),         "30000.00");
    EXPECT_EQ(body.at("size"),          "0.01");
    EXPECT_EQ(body.at("time_in_force"), "GTC");
    EXPECT_EQ(body.at("post_only"),     true);
    EXPECT_EQ(body.at("client_oid"),    "abc-123");
    EXPECT_FALSE(body.contains("funds"));
}

TEST(CoinbaseRestRequests, PlaceMarketOrder_FundsNoPrice) {
    CoinbasePlaceOrderRequest req;
    req.product_id = "BTC-USD";
    req.side       = Side::Buy;
    req.type       = OrderType::Market;
    req.funds      = "150.00";

    auto body = json::parse(req.build().body);
    EXPECT_EQ(body.at("type"),  "market");
    EXPECT_EQ(body.at("funds"), "150.00");
    EXPECT_FALSE(body.contains("price"));
    EXPECT_FALSE(body.contains("size"));
}

TEST(CoinbaseRestRequests, GetOrder) {
    CoinbaseGetOrderRequest req;
    req.order_id = "ord-1";
    EXPECT_EQ(req.build().path, "/orders/ord-1");
}

TEST(CoinbaseRestRequests, ListOrders_StatusAndProduct) {
    CoinbaseListOrdersRequest req;
    req.status     = "open";
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.path, "/orders");
    EXPECT_EQ(r.query, "status=open&product_id=BTC-USD");
}

TEST(CoinbaseRestRequests, CancelOrder) {
    CoinbaseCancelOrderRequest req;
    req.order_id   = "ord-1";
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.method, M::DELETE);
    EXPECT_EQ(r.path, "/orders/ord-1");
    EXPECT_EQ(r.query, "product_id=BTC-USD");
}

TEST(CoinbaseRestRequests, CancelAll_WithProduct) {
    CoinbaseCancelAllOrdersRequest req;
    req.product_id = "BTC-USD";
    auto r = req.build();
    EXPECT_EQ(r.method, M::DELETE);
    EXPECT_EQ(r.path, "/orders");
    EXPECT_EQ(r.query, "product_id=BTC-USD");
}

TEST(CoinbaseRestRequests, Fills_ByOrderId) {
    CoinbaseFillsRequest req;
    req.order_id = "ord-1";
    auto r = req.build();
    EXPECT_EQ(r.path, "/fills");
    EXPECT_EQ(r.query, "order_id=ord-1");
}
