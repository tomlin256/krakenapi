// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/common/ws_client.hpp
// Exchange-agnostic WebSocket client.
//
// ExchangeWsClient provides the complete typed request/subscription lifecycle
// — pending-handler map, push-subscription map, pre-connection queue,
// SubscriptionHandle, thread safety — parameterised by a per-exchange
// MessageIdentifier function that maps inbound JSON frames to FrameDescriptor.
//
// Factories:
//   make_exchange_ws_client(conn, identifier, error_handler)
//     — wraps an already-managed IWsConnection; callers call conn->connect()
//       themselves if they want to control the connection lifecycle.
//
// Usage (see exchange/common/ix_ws_connection.hpp for the URL-based factory):
//   auto conn   = std::make_shared<MyMockConn>();
//   auto client = exchange::ws::make_exchange_ws_client(
//       std::static_pointer_cast<exchange::ws::IWsConnection>(conn),
//       my_exchange_identifier);

#include "exchange/common/ws.hpp"

#include <future>
#include <map>
#include <mutex>
#include <vector>

namespace exchange::ws {

// ─────────────────────────────────────────────────────────────────────────────
// ExchangeWsClient
// ─────────────────────────────────────────────────────────────────────────────

class ExchangeWsClient : public std::enable_shared_from_this<ExchangeWsClient> {
public:
    ExchangeWsClient(std::shared_ptr<IWsConnection>  conn,
                     MessageIdentifier                identifier,
                     std::shared_ptr<IWsErrorHandler> error_handler = nullptr)
        : conn_(std::move(conn))
        , identifier_(std::move(identifier))
        , error_handler_(std::move(error_handler))
    {}

    // Must be called once after the shared_ptr<ExchangeWsClient> is created.
    // The factory functions handle this.
    void init();

    // ── Method calls (request → single typed response) ────────────────────

    template<typename Req>
    std::future<WsResponse<typename Req::response_type>>
    execute_async(Req req);

    template<typename Req>
    WsResponse<typename Req::response_type>
    execute(Req req,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    // ── Subscriptions (three-phase: open → ack → push stream) ─────────────
    //
    // Req must satisfy the subscribe-request concept (see ws.hpp):
    //   Req::push_type, Req::response_type, route_key(), to_json(), unsubscribe_json()

    template<typename Req>
    std::future<std::pair<WsResponse<typename Req::response_type>, SubscriptionHandle>>
    subscribe_async(Req req,
                    std::function<void(typename Req::push_type)> callback);

    template<typename Req>
    std::pair<WsResponse<typename Req::response_type>, SubscriptionHandle>
    subscribe(Req req,
              std::function<void(typename Req::push_type)> callback,
              std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    // Register a callback invoked when the WebSocket connection closes.
    void set_on_disconnect(std::function<void(std::string)> cb);

    // Called by SubscriptionHandle::cancel() to remove the push callback
    // and transmit the unsubscribe frame.
    void cancel_subscription(const std::string& route_key,
                             const std::string& unsub_json);

private:
    std::shared_ptr<IWsConnection>   conn_;
    MessageIdentifier                identifier_;
    std::shared_ptr<IWsErrorHandler> error_handler_;
    std::function<void(std::string)> disconnect_cb_;
    std::atomic<int64_t>             next_req_id_{1};
    std::atomic<bool>                connected_{false};

    std::mutex               queue_mu_;
    std::vector<std::string> send_queue_;

    // Pending method-call / subscribe-ack handlers keyed by string correlation id.
    // Kraken stringifies its int64_t req_id; Binance uses its id string directly.
    std::mutex                                              pending_mu_;
    std::map<std::string, std::function<void(const json&)>> pending_;

    std::mutex                                              subs_mu_;
    std::map<std::string, std::function<void(const json&)>> subscriptions_;

    int64_t gen_req_id() { return next_req_id_.fetch_add(1); }

    void enqueue_or_send(const std::string& msg);
    void on_open_handler();
    void on_raw_message(const std::string& raw);
};

} // namespace exchange::ws

#include "exchange/common/ws_client.inl"
