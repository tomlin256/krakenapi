// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// kraken_ws_client.inl — non-template inline implementations and template method
// implementations for KrakenWsClient. Included at the bottom of
// kraken_ws_client.hpp; do not include directly.

#pragma once

namespace kraken::ws {

// ─────────────────────────────────────────────────────────────────────────────
// RateLimitedWsErrorHandler
// ─────────────────────────────────────────────────────────────────────────────

inline RateLimitedWsErrorHandler::RateLimitedWsErrorHandler(
    std::chrono::milliseconds interval)
    : interval_us_(std::chrono::duration_cast<std::chrono::microseconds>(interval).count())
{}

inline void RateLimitedWsErrorHandler::on_malformed_frame(
    const std::string& /*raw*/, const std::exception& e)
{
    const auto count  = ++count_;
    const auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto last   = last_logged_us_.load(std::memory_order_relaxed);

    if (count == 1 || now_us - last >= interval_us_) {
        last_logged_us_.store(now_us, std::memory_order_relaxed);
        std::fprintf(stderr,
            "KrakenWsClient: failed to parse WebSocket frame"
            " (total malformed: %llu) — %s\n",
            static_cast<unsigned long long>(count), e.what());
    }
}

inline void RateLimitedWsErrorHandler::on_connection_error(const std::string& reason)
{
    std::fprintf(stderr,
        "KrakenWsClient: WebSocket connection error — %s\n",
        reason.c_str());
}

// ─────────────────────────────────────────────────────────────────────────────
// detail::make_ws_response<T>
// ─────────────────────────────────────────────────────────────────────────────

namespace detail {

template<typename T>
WsResponse<T> make_ws_response(T r) {
    WsResponse<T> ws;
    if constexpr (std::is_base_of_v<BaseResponse, T>) {
        ws.ok    = r.success;
        ws.error = r.error;
    } else {
        ws.ok = true;
    }
    ws.result = std::move(r);
    return ws;
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// KrakenWsClient template methods
// ─────────────────────────────────────────────────────────────────────────────

template<typename Req>
std::future<WsResponse<typename Req::response_type>>
KrakenWsClient::execute_async(Req req) {
    using Resp = typename Req::response_type;

    auto id   = gen_req_id();
    req.req_id = id;

    auto prom = std::make_shared<std::promise<WsResponse<Resp>>>();
    auto fut  = prom->get_future();

    {
        std::lock_guard<std::mutex> lk(pending_mu_);
        pending_[id] = [prom](const json& j) {
            prom->set_value(detail::make_ws_response(Resp::from_json(j)));
        };
    }

    enqueue_or_send(req.to_json().dump());
    return fut;
}

template<typename Req>
WsResponse<typename Req::response_type>
KrakenWsClient::execute(Req req, std::chrono::milliseconds timeout) {
    auto fut = execute_async(std::move(req));
    if (fut.wait_for(timeout) == std::future_status::timeout) {
        WsResponse<typename Req::response_type> err;
        err.ok    = false;
        err.error = "request timed out";
        return err;
    }
    return fut.get();
}

template<typename Req>
std::future<std::pair<WsResponse<SubscribeResponse>, SubscriptionHandle>>
KrakenWsClient::subscribe_async(Req req,
                                std::function<void(typename Req::push_type)> callback) {
    using PushMsg = typename Req::push_type;

    auto id  = gen_req_id();
    req.req_id = id;

    const std::string ch = to_string(req.channel);

    // Pre-build the unsubscribe message so the handle can send it later.
    UnsubscribeRequest unsub;
    unsub.channel = req.channel;
    unsub.symbols = req.symbols;
    unsub.token   = req.token;
    std::string unsub_json = unsub.to_json().dump();

    // Type-erased push callback; installed only after a successful ack.
    auto erased_push = std::make_shared<std::function<void(const json&)>>(
        [callback](const json& j) { callback(PushMsg::from_json(j)); }
    );

    auto active = std::make_shared<std::atomic<bool>>(false);

    auto prom = std::make_shared<
        std::promise<std::pair<WsResponse<SubscribeResponse>, SubscriptionHandle>>
    >();
    auto fut = prom->get_future();

    // The ack handler runs from on_raw_message, which is only invoked while
    // a shared_ptr<KrakenWsClient> is held by the ixwebsocket callback, so
    // capturing raw 'this' is safe here.
    {
        std::lock_guard<std::mutex> lk(pending_mu_);
        pending_[id] = [this, prom, ch, erased_push, active,
                        unsub_json = std::move(unsub_json)](const json& j) mutable
        {
            auto ack = SubscribeResponse::from_json(j);
            WsResponse<SubscribeResponse> ws;
            ws.ok     = ack.success;
            ws.error  = ack.error;
            ws.result = ack;

            SubscriptionHandle handle;
            if (ack.success) {
                // Phase 3 success: install the push callback.
                active->store(true);
                {
                    std::lock_guard<std::mutex> slk(subs_mu_);
                    subscriptions_[ch] = *erased_push;
                }
                handle = SubscriptionHandle(
                    active,
                    std::weak_ptr<KrakenWsClient>(shared_from_this()),
                    ch,
                    std::move(unsub_json)
                );
            }
            prom->set_value({std::move(ws), std::move(handle)});
        };
    }

    enqueue_or_send(req.to_json().dump());
    return fut;
}

template<typename Req>
std::pair<WsResponse<SubscribeResponse>, SubscriptionHandle>
KrakenWsClient::subscribe(Req req,
                           std::function<void(typename Req::push_type)> callback,
                           std::chrono::milliseconds timeout) {
    auto fut = subscribe_async(std::move(req), std::move(callback));
    if (fut.wait_for(timeout) == std::future_status::timeout) {
        WsResponse<SubscribeResponse> err;
        err.ok    = false;
        err.error = "subscribe timed out";
        return {std::move(err), SubscriptionHandle{}};
    }
    return fut.get();
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory functions
// ─────────────────────────────────────────────────────────────────────────────

inline std::shared_ptr<KrakenWsClient>
make_ws_client(std::shared_ptr<IWsConnection>   conn,
               std::shared_ptr<IWsErrorHandler>  error_handler = nullptr) {
    auto client = std::make_shared<KrakenWsClient>(std::move(conn), std::move(error_handler));
    client->init();
    return client;
}

} // namespace kraken::ws
