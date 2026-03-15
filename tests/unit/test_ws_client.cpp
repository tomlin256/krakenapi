// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Unit tests for KrakenWsClient using a MockWsConnection that avoids
// any real network I/O.  The mock lets tests:
//   - Inspect outbound messages (conn->sent_messages)
//   - Inject inbound server frames (conn->inject_message(...))
//   - Simulate connection open/close (conn->fire_open() / fire_close())

#include "kraken_ws_client.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
// MockWsConnection
// ─────────────────────────────────────────────────────────────────────────────

class MockWsConnection : public kraken::ws::IWsConnection {
public:
    std::vector<std::string> sent_messages;

    // connect() marks the connection as internally ready but does NOT fire
    // on_open automatically – tests call fire_open() for full control.
    void connect()    override { connected_ = true; }
    void disconnect() override { connected_ = false; }
    bool is_connected() const override { return connected_; }

    void send(const std::string& msg) override {
        sent_messages.push_back(msg);
    }

    void set_on_message(MessageCb cb) override { msg_cb_  = std::move(cb); }
    void set_on_open(OpenCb cb)       override { open_cb_ = std::move(cb); }
    void set_on_close(CloseCb cb)     override { close_cb_= std::move(cb); }

    // Test helpers
    void inject_message(const std::string& raw) { if (msg_cb_)   msg_cb_(raw);  }
    void fire_open()                             { if (open_cb_)  open_cb_();    }
    void fire_close()                            { if (close_cb_) close_cb_();   }

private:
    bool      connected_{false};
    MessageCb msg_cb_;
    OpenCb    open_cb_;
    CloseCb   close_cb_;
};

// ─────────────────────────────────────────────────────────────────────────────
// Test factory: creates a client backed by a MockWsConnection, already "open"
// ─────────────────────────────────────────────────────────────────────────────

static std::pair<std::shared_ptr<kraken::ws::KrakenWsClient>,
                 std::shared_ptr<MockWsConnection>>
make_test_client() {
    auto conn   = std::make_shared<MockWsConnection>();
    auto client = std::make_shared<kraken::ws::KrakenWsClient>(conn);
    client->init();
    conn->fire_open();   // simulate immediate connection
    return {client, conn};
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build a minimal AddOrderResponse JSON with matching req_id
// ─────────────────────────────────────────────────────────────────────────────

static std::string make_add_order_response(int64_t req_id,
                                           bool success = true,
                                           const std::string& order_id = "ORDER-001",
                                           const std::string& error_msg = "") {
    json j;
    j["method"]  = "add_order";
    j["req_id"]  = req_id;
    j["success"] = success;
    if (success) {
        j["result"]["order_id"] = order_id;
    } else {
        j["error"] = error_msg;
    }
    return j.dump();
}

static std::string make_subscribe_ack(int64_t req_id,
                                      bool success = true,
                                      const std::string& channel = "ticker",
                                      const std::string& error_msg = "") {
    json j;
    j["method"]  = "subscribe";
    j["req_id"]  = req_id;
    j["success"] = success;
    if (success) {
        j["result"]["channel"] = channel;
    } else {
        j["error"] = error_msg;
    }
    return j.dump();
}

static std::string make_ticker_push(const std::string& symbol = "BTC/USD",
                                    double bid = 50000.0,
                                    double ask = 50001.0) {
    json j;
    j["channel"] = "ticker";
    j["type"]    = "update";
    j["data"]    = json::array();
    json d;
    d["symbol"]   = symbol;
    d["bid"]      = bid;
    d["bid_qty"]  = 1.0;
    d["ask"]      = ask;
    d["ask_qty"]  = 1.0;
    d["last"]     = bid;
    d["volume"]   = 100.0;
    d["vwap"]     = bid;
    d["low"]      = bid - 500;
    d["high"]     = bid + 500;
    d["change"]   = 0.0;
    d["change_pct"] = 0.0;
    j["data"].push_back(d);
    return j.dump();
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: TypedWsRequest – compile-time type aliases
// ─────────────────────────────────────────────────────────────────────────────

TEST(TypedWsRequest, ResponseTypeAliases) {
    static_assert(std::is_same_v<kraken::ws::AddOrderRequest::response_type,
                                 kraken::ws::AddOrderResponse>);
    static_assert(std::is_same_v<kraken::ws::AmendOrderRequest::response_type,
                                 kraken::ws::AmendOrderResponse>);
    static_assert(std::is_same_v<kraken::ws::CancelOrderRequest::response_type,
                                 kraken::ws::CancelOrderResponse>);
    static_assert(std::is_same_v<kraken::ws::CancelAllRequest::response_type,
                                 kraken::ws::CancelAllResponse>);
    static_assert(std::is_same_v<kraken::ws::CancelOnDisconnectRequest::response_type,
                                 kraken::ws::CancelOnDisconnectResponse>);
    static_assert(std::is_same_v<kraken::ws::BatchAddRequest::response_type,
                                 kraken::ws::BatchAddResponse>);
    static_assert(std::is_same_v<kraken::ws::BatchCancelRequest::response_type,
                                 kraken::ws::BatchCancelResponse>);
    static_assert(std::is_same_v<kraken::ws::EditOrderRequest::response_type,
                                 kraken::ws::EditOrderResponse>);
    static_assert(std::is_same_v<kraken::ws::PingRequest::response_type,
                                 kraken::ws::PongMessage>);
    SUCCEED();  // all static_asserts passed
}

TEST(TypedSubscribeRequest, PushAndResponseTypes) {
    static_assert(std::is_same_v<kraken::ws::TickerSubscribeRequest::push_type,
                                 kraken::ws::TickerMessage>);
    static_assert(std::is_same_v<kraken::ws::TickerSubscribeRequest::response_type,
                                 kraken::ws::SubscribeResponse>);

    static_assert(std::is_same_v<kraken::ws::BookSubscribeRequest::push_type,
                                 kraken::ws::BookMessage>);
    static_assert(std::is_same_v<kraken::ws::OHLCSubscribeRequest::push_type,
                                 kraken::ws::OHLCMessage>);
    static_assert(std::is_same_v<kraken::ws::TradeSubscribeRequest::push_type,
                                 kraken::ws::TradeMessage>);
    static_assert(std::is_same_v<kraken::ws::ExecutionsSubscribeRequest::push_type,
                                 kraken::ws::ExecutionsMessage>);
    static_assert(std::is_same_v<kraken::ws::BalancesSubscribeRequest::push_type,
                                 kraken::ws::BalancesMessage>);
    SUCCEED();
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: execute_async
// ─────────────────────────────────────────────────────────────────────────────

TEST(ExecuteAsync, SendsRequestWithInjectedReqId) {
    auto [client, conn] = make_test_client();

    kraken::ws::AddOrderRequest req;
    req.order_type = kraken::OrderType::Limit;
    req.side       = kraken::Side::Buy;
    req.order_qty  = 0.001;
    req.symbol     = "BTC/USD";
    req.token      = "tok";
    req.limit_price = 30000.0;

    auto fut = client->execute_async(req);

    ASSERT_EQ(conn->sent_messages.size(), 1u);
    auto j = json::parse(conn->sent_messages[0]);
    EXPECT_TRUE(j.contains("req_id"));
    EXPECT_EQ(j["method"], "add_order");

    // Satisfy the future to avoid broken-promise at shutdown.
    int64_t id = j["req_id"].get<int64_t>();
    conn->inject_message(make_add_order_response(id));
    fut.get();
}

TEST(ExecuteAsync, ResolvesOnMatchingSuccessResponse) {
    auto [client, conn] = make_test_client();

    kraken::ws::AddOrderRequest req;
    req.order_type  = kraken::OrderType::Market;
    req.side        = kraken::Side::Sell;
    req.order_qty   = 0.1;
    req.symbol      = "BTC/USD";
    req.token       = "tok";

    auto fut = client->execute_async(req);
    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();

    conn->inject_message(make_add_order_response(id, true, "ORDER-XYZ"));

    auto resp = fut.get();
    EXPECT_TRUE(resp.ok);
    ASSERT_TRUE(resp.result.has_value());
    EXPECT_EQ(resp.result->order_id.value_or(""), "ORDER-XYZ");
}

TEST(ExecuteAsync, ResolvesOnErrorResponse) {
    auto [client, conn] = make_test_client();

    kraken::ws::AddOrderRequest req;
    req.order_type = kraken::OrderType::Limit;
    req.side       = kraken::Side::Buy;
    req.order_qty  = 0.001;
    req.symbol     = "BTC/USD";
    req.token      = "tok";
    req.limit_price = 1.0;

    auto fut = client->execute_async(req);
    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();

    conn->inject_message(make_add_order_response(id, false, "", "EOrder:Insufficient funds"));

    auto resp = fut.get();
    EXPECT_FALSE(resp.ok);
    EXPECT_EQ(resp.error.value_or(""), "EOrder:Insufficient funds");
    EXPECT_FALSE(resp.result->order_id.has_value());
}

TEST(ExecuteAsync, TwoRequestsInFlightResolvedInReverseOrder) {
    auto [client, conn] = make_test_client();

    kraken::ws::AddOrderRequest req1;
    req1.order_type = kraken::OrderType::Limit;
    req1.side       = kraken::Side::Buy;
    req1.order_qty  = 0.001;
    req1.symbol     = "BTC/USD";
    req1.token      = "tok";
    req1.limit_price = 30000.0;

    kraken::ws::AddOrderRequest req2 = req1;
    req2.limit_price = 31000.0;

    auto fut1 = client->execute_async(req1);
    auto fut2 = client->execute_async(req2);

    ASSERT_EQ(conn->sent_messages.size(), 2u);
    int64_t id1 = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    int64_t id2 = json::parse(conn->sent_messages[1])["req_id"].get<int64_t>();
    EXPECT_NE(id1, id2);

    // Inject responses in reverse order.
    conn->inject_message(make_add_order_response(id2, true, "ORDER-B"));
    conn->inject_message(make_add_order_response(id1, true, "ORDER-A"));

    auto resp1 = fut1.get();
    auto resp2 = fut2.get();

    EXPECT_EQ(resp1.result->order_id.value_or(""), "ORDER-A");
    EXPECT_EQ(resp2.result->order_id.value_or(""), "ORDER-B");
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: execute (sync / blocking)
// ─────────────────────────────────────────────────────────────────────────────

TEST(Execute, BlocksUntilResponse) {
    auto [client, conn] = make_test_client();
    auto& conn_ref = conn;

    kraken::ws::AddOrderRequest req;
    req.order_type  = kraken::OrderType::Limit;
    req.side        = kraken::Side::Buy;
    req.order_qty   = 0.001;
    req.symbol      = "BTC/USD";
    req.token       = "tok";
    req.limit_price  = 30000.0;

    // Inject the response from a separate thread after a short delay.
    std::thread injector([&] {
        std::this_thread::sleep_for(50ms);
        int64_t id = json::parse(conn_ref->sent_messages[0])["req_id"].get<int64_t>();
        conn_ref->inject_message(make_add_order_response(id, true, "ORDER-SYNC"));
    });

    auto resp = client->execute(req, 2000ms);
    injector.join();

    EXPECT_TRUE(resp.ok);
    EXPECT_EQ(resp.result->order_id.value_or(""), "ORDER-SYNC");
}

TEST(Execute, TimesOutAndReturnsError) {
    auto [client, conn] = make_test_client();

    kraken::ws::AddOrderRequest req;
    req.order_type  = kraken::OrderType::Limit;
    req.side        = kraken::Side::Buy;
    req.order_qty   = 0.001;
    req.symbol      = "BTC/USD";
    req.token       = "tok";
    req.limit_price  = 30000.0;

    // No response injected → should time out.
    auto resp = client->execute(req, 50ms);

    EXPECT_FALSE(resp.ok);
    EXPECT_EQ(resp.error.value_or(""), "request timed out");
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: subscribe (three-phase lifecycle)
// ─────────────────────────────────────────────────────────────────────────────

TEST(Subscribe, PushCallbackNotFiredBeforeAck) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{"BTC/USD"};

    std::atomic<int> push_count{0};

    auto fut = client->subscribe_async(
        sub_req,
        [&](const kraken::ws::TickerMessage&) { ++push_count; }
    );

    // Inject push data BEFORE the ack – callback must not fire.
    conn->inject_message(make_ticker_push());
    EXPECT_EQ(push_count.load(), 0);

    // Now send the ack.
    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id));
    auto [ack, handle] = fut.get();

    EXPECT_TRUE(ack.ok);
    EXPECT_EQ(push_count.load(), 0);  // push before ack still not counted
}

TEST(Subscribe, SuccessfulAckThenPushData) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{"BTC/USD"};

    std::atomic<int>    push_count{0};
    std::atomic<double> last_bid{0.0};

    auto fut = client->subscribe_async(
        sub_req,
        [&](const kraken::ws::TickerMessage& msg) {
            ++push_count;
            if (!msg.data.empty()) last_bid.store(msg.data[0].bid);
        }
    );

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id));
    auto [ack, handle] = fut.get();

    EXPECT_TRUE(ack.ok);
    EXPECT_TRUE(handle.is_active());

    // Push data AFTER ack – callback must fire.
    conn->inject_message(make_ticker_push("BTC/USD", 55000.0, 55001.0));
    EXPECT_EQ(push_count.load(), 1);
    EXPECT_DOUBLE_EQ(last_bid.load(), 55000.0);

    conn->inject_message(make_ticker_push("BTC/USD", 56000.0, 56001.0));
    EXPECT_EQ(push_count.load(), 2);
}

TEST(Subscribe, FailedAckPushCallbackNeverFires) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{"BTC/USD"};

    std::atomic<int> push_count{0};

    auto fut = client->subscribe_async(
        sub_req,
        [&](const kraken::ws::TickerMessage&) { ++push_count; }
    );

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id, false, "ticker", "Permission denied"));
    auto [ack, handle] = fut.get();

    EXPECT_FALSE(ack.ok);
    EXPECT_EQ(ack.error.value_or(""), "Permission denied");

    // Push data after failed ack – callback must never fire.
    conn->inject_message(make_ticker_push());
    EXPECT_EQ(push_count.load(), 0);
}

TEST(Subscribe, HandleIsInactiveAfterFailedAck) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;

    auto fut = client->subscribe_async(
        sub_req, [](const kraken::ws::TickerMessage&) {}
    );

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id, false, "ticker", "Unauthorized"));
    auto [ack, handle] = fut.get();

    EXPECT_FALSE(handle.is_active());
}

TEST(Subscribe, HandleIsActiveAfterSuccessfulAck) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;

    auto fut = client->subscribe_async(
        sub_req, [](const kraken::ws::TickerMessage&) {}
    );

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id));
    auto [ack, handle] = fut.get();

    EXPECT_TRUE(handle.is_active());
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: SubscriptionHandle::cancel
// ─────────────────────────────────────────────────────────────────────────────

TEST(Cancel, RemovesPushCallback) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;

    std::atomic<int> push_count{0};

    auto fut = client->subscribe_async(
        sub_req,
        [&](const kraken::ws::TickerMessage&) { ++push_count; }
    );

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id));
    auto [ack, handle] = fut.get();
    ASSERT_TRUE(ack.ok);

    // Confirm push fires before cancel.
    conn->inject_message(make_ticker_push());
    EXPECT_EQ(push_count.load(), 1);

    handle.cancel();
    EXPECT_FALSE(handle.is_active());

    // Push after cancel must not fire.
    conn->inject_message(make_ticker_push());
    EXPECT_EQ(push_count.load(), 1);
}

TEST(Cancel, SendsUnsubscribeRequest) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{"BTC/USD"};

    auto fut = client->subscribe_async(
        sub_req, [](const kraken::ws::TickerMessage&) {}
    );

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id));
    auto [ack, handle] = fut.get();
    ASSERT_TRUE(ack.ok);

    const size_t msg_count_before = conn->sent_messages.size();
    handle.cancel();

    ASSERT_GT(conn->sent_messages.size(), msg_count_before);
    auto unsub_j = json::parse(conn->sent_messages.back());
    EXPECT_EQ(unsub_j["method"], "unsubscribe");
    EXPECT_EQ(unsub_j["params"]["channel"], "ticker");
}

TEST(Cancel, IsIdempotent) {
    auto [client, conn] = make_test_client();

    kraken::ws::TickerSubscribeRequest sub_req;

    auto fut = client->subscribe_async(
        sub_req, [](const kraken::ws::TickerMessage&) {}
    );

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id));
    auto [ack, handle] = fut.get();

    const size_t before = conn->sent_messages.size();
    handle.cancel();
    handle.cancel();  // second call should be a no-op
    handle.cancel();

    // Only one unsubscribe should have been sent.
    EXPECT_EQ(conn->sent_messages.size(), before + 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: pre-connection outbound queue
// ─────────────────────────────────────────────────────────────────────────────

TEST(PreConnectionQueue, MessagesQueuedBeforeOpen) {
    auto conn   = std::make_shared<MockWsConnection>();
    auto client = std::make_shared<kraken::ws::KrakenWsClient>(conn);
    client->init();
    // Deliberately NOT calling conn->fire_open() yet.

    kraken::ws::AddOrderRequest req;
    req.order_type  = kraken::OrderType::Limit;
    req.side        = kraken::Side::Buy;
    req.order_qty   = 0.001;
    req.symbol      = "BTC/USD";
    req.token       = "tok";
    req.limit_price  = 30000.0;

    auto fut = client->execute_async(req);

    // Nothing should have been sent yet.
    EXPECT_TRUE(conn->sent_messages.empty());

    // Simulate connection opening → queue flushed.
    conn->fire_open();
    ASSERT_EQ(conn->sent_messages.size(), 1u);

    // Satisfy future.
    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_add_order_response(id));
    fut.get();
}

TEST(PreConnectionQueue, FlushedInOrder) {
    auto conn   = std::make_shared<MockWsConnection>();
    auto client = std::make_shared<kraken::ws::KrakenWsClient>(conn);
    client->init();

    kraken::ws::AddOrderRequest req1;
    req1.order_type = kraken::OrderType::Limit;
    req1.side       = kraken::Side::Buy;
    req1.order_qty  = 0.001;
    req1.symbol     = "BTC/USD";
    req1.token      = "tok";
    req1.limit_price = 30000.0;

    kraken::ws::AddOrderRequest req2 = req1;
    req2.limit_price = 31000.0;

    auto fut1 = client->execute_async(req1);
    auto fut2 = client->execute_async(req2);

    ASSERT_TRUE(conn->sent_messages.empty());

    conn->fire_open();

    ASSERT_EQ(conn->sent_messages.size(), 2u);

    auto j1 = json::parse(conn->sent_messages[0]);
    auto j2 = json::parse(conn->sent_messages[1]);

    // Both are add_order requests.
    EXPECT_EQ(j1["method"], "add_order");
    EXPECT_EQ(j2["method"], "add_order");

    // They have distinct req_ids.
    EXPECT_NE(j1["req_id"].get<int64_t>(), j2["req_id"].get<int64_t>());

    // Satisfy futures.
    conn->inject_message(make_add_order_response(j1["req_id"].get<int64_t>()));
    conn->inject_message(make_add_order_response(j2["req_id"].get<int64_t>()));
    fut1.get();
    fut2.get();
}

TEST(PreConnectionQueue, DirectSendWhenAlreadyConnected) {
    // make_test_client fires open immediately, so sends go directly.
    auto [client, conn] = make_test_client();

    kraken::ws::AddOrderRequest req;
    req.order_type  = kraken::OrderType::Limit;
    req.side        = kraken::Side::Buy;
    req.order_qty   = 0.001;
    req.symbol      = "BTC/USD";
    req.token       = "tok";
    req.limit_price  = 30000.0;

    auto fut = client->execute_async(req);

    // Should have sent immediately (no need for fire_open).
    ASSERT_EQ(conn->sent_messages.size(), 1u);

    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_add_order_response(id));
    fut.get();
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: TypedSubscribeRequest channel binding
// ─────────────────────────────────────────────────────────────────────────────

TEST(TypedSubscribeRequest, ChannelIsBoundToType) {
    static_assert(kraken::ws::TickerSubscribeRequest::channel_value     == kraken::ws::SubscribeChannel::Ticker);
    static_assert(kraken::ws::BookSubscribeRequest::channel_value       == kraken::ws::SubscribeChannel::Book);
    static_assert(kraken::ws::TradeSubscribeRequest::channel_value      == kraken::ws::SubscribeChannel::Trade);
    static_assert(kraken::ws::OHLCSubscribeRequest::channel_value       == kraken::ws::SubscribeChannel::OHLC);
    static_assert(kraken::ws::InstrumentSubscribeRequest::channel_value == kraken::ws::SubscribeChannel::Instrument);
    static_assert(kraken::ws::ExecutionsSubscribeRequest::channel_value == kraken::ws::SubscribeChannel::Executions);
    static_assert(kraken::ws::BalancesSubscribeRequest::channel_value   == kraken::ws::SubscribeChannel::Balances);
    SUCCEED();
}

TEST(TypedSubscribeRequest, DefaultConstructorSetsChannelField) {
    EXPECT_EQ(kraken::ws::TickerSubscribeRequest{}.channel,     kraken::ws::SubscribeChannel::Ticker);
    EXPECT_EQ(kraken::ws::BookSubscribeRequest{}.channel,       kraken::ws::SubscribeChannel::Book);
    EXPECT_EQ(kraken::ws::TradeSubscribeRequest{}.channel,      kraken::ws::SubscribeChannel::Trade);
    EXPECT_EQ(kraken::ws::OHLCSubscribeRequest{}.channel,       kraken::ws::SubscribeChannel::OHLC);
    EXPECT_EQ(kraken::ws::InstrumentSubscribeRequest{}.channel, kraken::ws::SubscribeChannel::Instrument);
    EXPECT_EQ(kraken::ws::ExecutionsSubscribeRequest{}.channel, kraken::ws::SubscribeChannel::Executions);
    EXPECT_EQ(kraken::ws::BalancesSubscribeRequest{}.channel,   kraken::ws::SubscribeChannel::Balances);
}

TEST(TypedSubscribeRequest, ToJsonContainsCorrectChannel) {
    kraken::ws::TickerSubscribeRequest req;
    req.symbols = std::vector<std::string>{"BTC/USD"};
    // No manual channel assignment — channel is set by the constructor.
    auto j = req.to_json();
    EXPECT_EQ(j["params"]["channel"].get<std::string>(), "ticker");
}

// ─────────────────────────────────────────────────────────────────────────────
// Group: IWsErrorHandler — malformed frame handling
// ─────────────────────────────────────────────────────────────────────────────

// A recording error handler that captures every call for inspection.
class RecordingErrorHandler : public kraken::ws::IWsErrorHandler {
public:
    struct Call { std::string raw; std::string what; };
    std::vector<Call> calls;

    void on_malformed_frame(const std::string& raw,
                            const std::exception& e) override {
        calls.push_back({raw, e.what()});
    }
};

// Helper: create a client with a RecordingErrorHandler already open.
static std::pair<std::shared_ptr<kraken::ws::KrakenWsClient>,
                 std::shared_ptr<MockWsConnection>>
make_client_with_handler(std::shared_ptr<kraken::ws::IWsErrorHandler> handler) {
    auto conn   = std::make_shared<MockWsConnection>();
    auto client = kraken::ws::make_ws_client(
        std::static_pointer_cast<kraken::ws::IWsConnection>(conn),
        std::move(handler));
    conn->fire_open();
    return {client, conn};
}

TEST(WsErrorHandler, InvokedOnMalformedFrame) {
    auto handler = std::make_shared<RecordingErrorHandler>();
    auto [client, conn] = make_client_with_handler(handler);

    conn->inject_message("not valid json {{{");

    ASSERT_EQ(handler->calls.size(), 1u);
    EXPECT_EQ(handler->calls[0].raw, "not valid json {{{");
    EXPECT_FALSE(handler->calls[0].what.empty());
}

TEST(WsErrorHandler, NotInvokedOnValidFrame) {
    auto handler = std::make_shared<RecordingErrorHandler>();
    auto [client, conn] = make_client_with_handler(handler);

    conn->inject_message(R"({"channel":"heartbeat"})");

    EXPECT_TRUE(handler->calls.empty());
}

TEST(WsErrorHandler, InvokedForEachMalformedFrame) {
    auto handler = std::make_shared<RecordingErrorHandler>();
    auto [client, conn] = make_client_with_handler(handler);

    conn->inject_message("bad1");
    conn->inject_message("bad2");
    conn->inject_message("bad3");

    EXPECT_EQ(handler->calls.size(), 3u);
    EXPECT_EQ(handler->calls[0].raw, "bad1");
    EXPECT_EQ(handler->calls[1].raw, "bad2");
    EXPECT_EQ(handler->calls[2].raw, "bad3");
}

TEST(WsErrorHandler, NotInvokedOnValidPushFrame) {
    auto handler = std::make_shared<RecordingErrorHandler>();
    auto [client, conn] = make_client_with_handler(handler);

    // Subscribe so the push callback is installed.
    kraken::ws::TickerSubscribeRequest sub_req;
    sub_req.symbols = std::vector<std::string>{"BTC/USD"};
    auto fut = client->subscribe_async(sub_req, [](const kraken::ws::TickerMessage&) {});
    int64_t id = json::parse(conn->sent_messages[0])["req_id"].get<int64_t>();
    conn->inject_message(make_subscribe_ack(id));
    fut.get();

    conn->inject_message(make_ticker_push());

    EXPECT_TRUE(handler->calls.empty());
}

TEST(WsErrorHandler, RateLimitedHandlerCanBeConstructedWithCustomInterval) {
    // Verify custom interval compiles and runs without crashing.
    auto handler = std::make_shared<kraken::ws::RateLimitedWsErrorHandler>(
        std::chrono::milliseconds{100});
    auto [client, conn] = make_client_with_handler(handler);

    for (int i = 0; i < 5; ++i)
        conn->inject_message("not json");
    // No crash, no throw.
    SUCCEED();
}

TEST(WsErrorHandler, DefaultHandlerUsedWhenNoneProvided) {
    // When no handler is passed, make_ws_client must not crash on bad input.
    auto conn   = std::make_shared<MockWsConnection>();
    auto client = kraken::ws::make_ws_client(
        std::static_pointer_cast<kraken::ws::IWsConnection>(conn));
    conn->fire_open();

    // Should not throw or crash — default RateLimitedWsErrorHandler handles it.
    EXPECT_NO_THROW(conn->inject_message("{bad json"));
}
