#pragma once

// kraken_ws_client.hpp
// Type-safe WebSocket client for the Kraken WebSocket API v2.
//
// Architecture mirrors KrakenRestClient:
//   execute<Req>(req)             – blocking method call, returns WsResponse<Req::response_type>
//   execute_async<Req>(req)       – non-blocking, returns std::future<WsResponse<…>>
//   subscribe<Req>(req, cb)       – three-phase subscription (open → ack → push stream)
//   subscribe_async<Req>(req, cb) – non-blocking variant of subscribe
//
// The three-phase subscription lifecycle:
//   Phase 1: WebSocket connection opens (on_open fires)
//   Phase 2: SubscribeRequest is sent (queued internally if not yet connected)
//   Phase 3: SubscribeResponse ack is received and matched by req_id
//             → success: push data callback installed, SubscriptionHandle activated
//             → failure: push callback never installed, handle is inactive
//
// Connection abstraction:
//   IWsConnection  – abstract interface; no ixwebsocket dependency visible to callers.
//                    Include kraken_ix_ws_connection.hpp for the concrete ixwebsocket
//                    implementation (IxWsConnection) and the URL factory overload.
//
// Factory functions (this header):
//   make_ws_client(conn)  – wraps an already-managed IWsConnection
//
// Additional factory (kraken_ix_ws_connection.hpp):
//   make_ws_client(url)   – creates a fresh IxWsConnection and connects
//
// Usage:
//   #include "kraken_ix_ws_connection.hpp"
//   auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));
//
//   kraken::ws::TickerSubscribeRequest sub;
//   sub.symbols = {"BTC/USD"};
//   auto [ack, handle] = client->subscribe(sub, [](kraken::ws::TickerMessage m) { … });
//   if (!ack.ok) { /* handle error */ }
//
//   kraken::ws::AddOrderRequest order{ … };
//   auto resp = client->execute(order);  // blocks up to 5 s
//
//   handle.cancel();  // unsubscribes

#include "kraken_ws_api.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace kraken::ws {

// ─────────────────────────────────────────────────────────────────────────────
// WsResponse<T>  –  mirrors RestResponse<T>
// ─────────────────────────────────────────────────────────────────────────────

template<typename T>
struct WsResponse {
    bool ok{false};
    std::optional<std::string> error;
    std::optional<T>           result;
};

// ─────────────────────────────────────────────────────────────────────────────
// IWsConnection  –  abstract interface (no ixwebsocket symbols exposed)
// ─────────────────────────────────────────────────────────────────────────────

class IWsConnection {
public:
    using MessageCb = std::function<void(const std::string&)>;
    using OpenCb    = std::function<void()>;
    using CloseCb   = std::function<void()>;

    virtual ~IWsConnection() = default;

    // Start the connection (may be async; on_open fires when ready).
    virtual void connect()                    = 0;
    // Stop the connection (blocking – joins background threads).
    virtual void disconnect()                 = 0;
    virtual bool is_connected() const         = 0;
    virtual void send(const std::string& msg) = 0;

    // Register callbacks – must be called before connect().
    virtual void on_message(MessageCb cb) = 0;
    virtual void on_open(OpenCb cb)       = 0;
    virtual void on_close(CloseCb cb)     = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Forward declaration
// ─────────────────────────────────────────────────────────────────────────────

class KrakenWsClient;

// ─────────────────────────────────────────────────────────────────────────────
// SubscriptionHandle  –  returned by subscribe(); call cancel() to unsubscribe
// ─────────────────────────────────────────────────────────────────────────────

class SubscriptionHandle {
public:
    // Default-constructed handle is inactive (subscription failed or not yet created).
    SubscriptionHandle() = default;

    bool is_active() const { return active_ && active_->load(); }

    // Remove the push callback from the dispatch table and send an UnsubscribeRequest.
    // Idempotent – safe to call multiple times.
    void cancel();

private:
    std::shared_ptr<std::atomic<bool>> active_;
    std::weak_ptr<KrakenWsClient>      client_;
    std::string                        channel_;
    std::string                        unsub_json_;  // pre-serialised UnsubscribeRequest

    friend class KrakenWsClient;

    SubscriptionHandle(std::shared_ptr<std::atomic<bool>> active,
                       std::weak_ptr<KrakenWsClient>      client,
                       std::string                        channel,
                       std::string                        unsub_json)
        : active_(std::move(active))
        , client_(std::move(client))
        , channel_(std::move(channel))
        , unsub_json_(std::move(unsub_json))
    {}
};

// ─────────────────────────────────────────────────────────────────────────────
// Internal helper: build WsResponse<T> from a parsed response object.
// Handles both BaseResponse sub-types (have .success / .error) and
// plain types like PongMessage (always considered successful).
// ─────────────────────────────────────────────────────────────────────────────

namespace detail {

template<typename T>
WsResponse<T> make_ws_response(T r);

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// KrakenWsClient
// ─────────────────────────────────────────────────────────────────────────────

class KrakenWsClient : public std::enable_shared_from_this<KrakenWsClient> {
public:
    explicit KrakenWsClient(std::shared_ptr<IWsConnection> conn)
        : conn_(std::move(conn))
    {}

    // Must be called once after the shared_ptr<KrakenWsClient> is created
    // (i.e. after std::make_shared). The factory functions handle this.
    void init();

    // ── Method calls (request → single typed response) ────────────────────

    // Non-blocking: returns a future resolved when the server responds.
    template<typename Req>
    std::future<WsResponse<typename Req::response_type>>
    execute_async(Req req);

    // Blocking: waits up to `timeout` for the server response.
    // Returns WsResponse with ok=false and error="request timed out" on timeout.
    template<typename Req>
    WsResponse<typename Req::response_type>
    execute(Req req,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    // ── Subscriptions (three-phase: open → ack → push stream) ─────────────

    // Non-blocking: registers `callback` for push data and returns a future that
    // resolves with the Phase 3 SubscribeResponse ack.
    // The SubscriptionHandle in the pair is active iff ack.ok == true.
    template<typename Req>
    std::future<std::pair<WsResponse<SubscribeResponse>, SubscriptionHandle>>
    subscribe_async(Req req,
                    std::function<void(typename Req::push_type)> callback);

    // Blocking: waits up to `timeout` for the Phase 3 ack.
    template<typename Req>
    std::pair<WsResponse<SubscribeResponse>, SubscriptionHandle>
    subscribe(Req req,
              std::function<void(typename Req::push_type)> callback,
              std::chrono::milliseconds timeout = std::chrono::milliseconds{5000});

    // Called by SubscriptionHandle::cancel() to remove the push callback
    // and transmit an UnsubscribeRequest.
    void cancel_subscription(const std::string& channel, const std::string& unsub_json);

private:
    std::shared_ptr<IWsConnection> conn_;
    std::atomic<int64_t>           next_req_id_{1};
    std::atomic<bool>              connected_{false};

    // Outbound queue: messages sent before on_open are queued and flushed on connect.
    std::mutex               queue_mu_;
    std::vector<std::string> send_queue_;

    // Pending method-call / subscribe-ack handlers, keyed by req_id.
    // Entries are inserted before send and erased when the matching response arrives.
    std::mutex                                           pending_mu_;
    std::map<int64_t, std::function<void(const json&)>> pending_;

    // Active push subscription callbacks, keyed by channel string.
    // Entries are installed only after a successful Phase 3 ack.
    std::mutex                                              subs_mu_;
    std::map<std::string, std::function<void(const json&)>> subscriptions_;

    int64_t gen_req_id() { return next_req_id_.fetch_add(1); }

    // Send immediately if connected; otherwise queue for flush on on_open.
    // queue_mu_ serialises the connected_ check and the queue push to avoid
    // a race with on_open_handler().
    void enqueue_or_send(const std::string& msg);

    // Called by the registered on_open callback.
    // Atomically marks connected and drains the outbound queue.
    void on_open_handler();

    // Dispatch an incoming raw JSON frame to either:
    //   – a pending method-call / subscribe-ack handler (matched by req_id), or
    //   – an active push subscription callback (matched by channel name).
    void on_raw_message(const std::string& raw);
};

// ─────────────────────────────────────────────────────────────────────────────
// Factory functions
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr std::string_view PUBLIC_WS_URL  = "wss://ws.kraken.com/v2";
inline constexpr std::string_view PRIVATE_WS_URL = "wss://ws-auth.kraken.com/v2";

// Wrap an already-managed connection (avoids a new TCP handshake, or useful
// when injecting a mock connection for tests).
// If the connection is already open, subsequent execute/subscribe calls will
// send immediately rather than queuing.
// See kraken_ix_ws_connection.hpp for the overload that creates a fresh
// IxWsConnection from a URL string.
inline std::shared_ptr<KrakenWsClient>
make_ws_client(std::shared_ptr<IWsConnection> conn) {
    auto client = std::make_shared<KrakenWsClient>(conn);
    client->init();
    return client;
}

} // namespace kraken::ws

#include "kraken_ws_client.inl"
