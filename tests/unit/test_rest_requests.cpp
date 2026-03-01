#include "kraken_rest_api.hpp"

#include <gtest/gtest.h>
#include <string>

using namespace kraken::rest;

// ---------------------------------------------------------------------------
// Public requests — verify path, method, and query parameters.
// ---------------------------------------------------------------------------

TEST(PublicRequests, GetServerTime_Path) {
    GetServerTimeRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/Time");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(PublicRequests, GetSystemStatus_Path) {
    GetSystemStatusRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/SystemStatus");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
}

TEST(PublicRequests, GetTicker_NoPairs) {
    GetTickerRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/Ticker");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_TRUE(http.query.empty());
}

TEST(PublicRequests, GetTicker_WithPair) {
    GetTickerRequest req;
    req.pairs = {"XBTUSD"};
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/Ticker");
    EXPECT_EQ(http.query, "pair=XBTUSD");
}

TEST(PublicRequests, GetTicker_MultiplePairs) {
    GetTickerRequest req;
    req.pairs = {"XBTUSD", "ETHUSD"};
    auto http = req.build();
    EXPECT_EQ(http.query, "pair=XBTUSD,ETHUSD");
}

TEST(PublicRequests, GetOHLC_Path) {
    GetOHLCRequest req;
    req.pair = "XBTUSD";
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/OHLC");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_NE(http.query.find("pair=XBTUSD"), std::string::npos);
}

TEST(PublicRequests, GetOrderBook_Path) {
    GetOrderBookRequest req;
    req.pair = "XBTUSD";
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/Depth");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
    EXPECT_NE(http.query.find("pair=XBTUSD"), std::string::npos);
}

TEST(PublicRequests, GetRecentTrades_Path) {
    GetRecentTradesRequest req;
    req.pair = "XBTUSD";
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/Trades");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
}

TEST(PublicRequests, GetAssetInfo_Path) {
    GetAssetInfoRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/Assets");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
}

TEST(PublicRequests, GetAssetPairs_Path) {
    GetAssetPairsRequest req;
    auto http = req.build();
    EXPECT_EQ(http.path, "/0/public/AssetPairs");
    EXPECT_EQ(http.method, HttpRequest::Method::GET);
}

// ---------------------------------------------------------------------------
// Private requests — verify path, method, required headers, and nonce in body.
// ---------------------------------------------------------------------------

// Minimal test credentials (signing will produce valid-format headers even with dummy keys).
static Credentials make_dummy_creds() {
    // The secret must be valid base64; "dGVzdA==" decodes to "test".
    return Credentials{"test-api-key", "dGVzdA=="};
}

TEST(PrivateRequests, GetAccountBalance_Path) {
    GetAccountBalanceRequest req;
    auto http = req.build(make_dummy_creds());
    EXPECT_EQ(http.path, "/0/private/Balance");
    EXPECT_EQ(http.method, HttpRequest::Method::POST);
}

TEST(PrivateRequests, PrivateRequest_HasAuthHeaders) {
    GetAccountBalanceRequest req;
    auto http = req.build(make_dummy_creds());
    EXPECT_EQ(http.headers.count("API-Key"), 1u);
    EXPECT_EQ(http.headers.at("API-Key"), "test-api-key");
    EXPECT_EQ(http.headers.count("API-Sign"), 1u);
    EXPECT_FALSE(http.headers.at("API-Sign").empty());
    EXPECT_EQ(http.headers.at("Content-Type"), "application/x-www-form-urlencoded");
}

TEST(PrivateRequests, PrivateRequest_HasNonceInBody) {
    GetAccountBalanceRequest req;
    auto http = req.build(make_dummy_creds());
    EXPECT_NE(http.body.find("nonce="), std::string::npos);
}

TEST(PrivateRequests, GetOpenOrders_Path) {
    GetOpenOrdersRequest req;
    auto http = req.build(make_dummy_creds());
    EXPECT_EQ(http.path, "/0/private/OpenOrders");
}

TEST(PrivateRequests, GetClosedOrders_Path) {
    GetClosedOrdersRequest req;
    auto http = req.build(make_dummy_creds());
    EXPECT_EQ(http.path, "/0/private/ClosedOrders");
}

TEST(PrivateRequests, AddOrder_Path) {
    AddOrderRequest req;
    req.params.order_type = kraken::OrderType::Limit;
    req.params.side       = kraken::Side::Buy;
    req.params.symbol     = "XBTUSD";
    req.params.order_qty  = 0.001;
    req.params.limit_price = 30000.0;
    auto http = req.build(make_dummy_creds());
    EXPECT_EQ(http.path, "/0/private/AddOrder");
    EXPECT_EQ(http.method, HttpRequest::Method::POST);
    EXPECT_NE(http.body.find("pair=XBTUSD"), std::string::npos);
    EXPECT_NE(http.body.find("ordertype=limit"), std::string::npos);
    EXPECT_NE(http.body.find("type=buy"), std::string::npos);
}

TEST(PrivateRequests, CancelOrder_Path) {
    CancelOrderRequest req;
    req.txid = "OABC123";
    auto http = req.build(make_dummy_creds());
    EXPECT_EQ(http.path, "/0/private/CancelOrder");
    EXPECT_NE(http.body.find("txid=OABC123"), std::string::npos);
}

TEST(PrivateRequests, GetWebSocketsToken_Path) {
    GetWebSocketsTokenRequest req;
    auto http = req.build(make_dummy_creds());
    EXPECT_EQ(http.path, "/0/private/GetWebSocketsToken");
}

// ---------------------------------------------------------------------------
// Type system — verify response_type is linked correctly at compile time.
// ---------------------------------------------------------------------------

TEST(TypeSystem, PublicRequestResponseType) {
    // If this compiles, the type alias is correctly linked.
    static_assert(std::is_same_v<GetServerTimeRequest::response_type, ServerTime>);
    static_assert(std::is_same_v<GetTickerRequest::response_type, TickerResult>);
    static_assert(std::is_same_v<GetOHLCRequest::response_type, OHLCResult>);
    static_assert(std::is_same_v<GetOrderBookRequest::response_type, OrderBookResult>);
    static_assert(std::is_same_v<GetRecentTradesRequest::response_type, RecentTradesResult>);
    SUCCEED();
}

TEST(TypeSystem, PrivateRequestResponseType) {
    static_assert(std::is_same_v<GetAccountBalanceRequest::response_type, AccountBalanceResult>);
    static_assert(std::is_same_v<AddOrderRequest::response_type, AddOrderResult>);
    static_assert(std::is_same_v<CancelOrderRequest::response_type, CancelOrderResult>);
    static_assert(std::is_same_v<GetWebSocketsTokenRequest::response_type, WebSocketsTokenResult>);
    SUCCEED();
}

TEST(TypeSystem, PublicRequestIsBaseOf) {
    static_assert(std::is_base_of_v<PublicRequest, GetServerTimeRequest>);
    static_assert(std::is_base_of_v<PublicRequest, GetTickerRequest>);
    SUCCEED();
}

TEST(TypeSystem, PrivateRequestIsBaseOf) {
    static_assert(std::is_base_of_v<PrivateRequest, GetAccountBalanceRequest>);
    static_assert(std::is_base_of_v<PrivateRequest, AddOrderRequest>);
    SUCCEED();
}
