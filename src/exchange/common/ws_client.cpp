// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/common/ws_client.hpp"

namespace exchange::ws {

// ─────────────────────────────────────────────────────────────────────────────
// SubscriptionHandle::cancel()  is implemented in ws_client.inl
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// ExchangeWsClient
// ─────────────────────────────────────────────────────────────────────────────

void ExchangeWsClient::init() {
    if (!error_handler_)
        error_handler_ = std::make_shared<RateLimitedWsErrorHandler>();

    if (conn_->is_connected())
        connected_.store(true);

    auto weak_self = std::weak_ptr<ExchangeWsClient>(shared_from_this());

    conn_->set_on_message([weak_self](const std::string& raw) {
        if (auto self = weak_self.lock()) self->on_raw_message(raw);
    });
    conn_->set_on_open([weak_self]() {
        if (auto self = weak_self.lock()) self->on_open_handler();
    });
    conn_->set_on_close([weak_self](std::string reason) {
        if (auto self = weak_self.lock()) {
            self->connected_.store(false);
            if (self->disconnect_cb_) self->disconnect_cb_(std::move(reason));
        }
    });
    conn_->set_on_error([weak_self](const std::string& reason) {
        if (auto self = weak_self.lock())
            self->error_handler_->on_connection_error(reason);
    });
}

void ExchangeWsClient::set_on_disconnect(std::function<void(std::string)> cb) {
    disconnect_cb_ = std::move(cb);
}

void ExchangeWsClient::cancel_subscription(const std::string& route_key,
                                            const std::string& unsub_json) {
    {
        std::lock_guard<std::mutex> lk(subs_mu_);
        subscriptions_.erase(route_key);
    }
    enqueue_or_send(unsub_json);
}

void ExchangeWsClient::enqueue_or_send(const std::string& msg) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    if (connected_.load()) {
        conn_->send(msg);
    } else {
        send_queue_.push_back(msg);
    }
}

void ExchangeWsClient::on_open_handler() {
    std::vector<std::string> queued;
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        connected_.store(true);
        queued = std::move(send_queue_);
    }
    for (const auto& msg : queued)
        conn_->send(msg);
}

void ExchangeWsClient::on_raw_message(const std::string& raw) {
    json j;
    try { j = json::parse(raw); }
    catch (const std::exception& e) {
        error_handler_->on_malformed_frame(raw, e);
        return;
    }

    const auto desc = identifier_(j);

    switch (desc.kind) {
        case FrameKind::MethodResponse: {
            if (!desc.correlation_id.has_value()) break;
            const auto& cid = *desc.correlation_id;
            std::function<void(const json&)> handler;
            {
                std::lock_guard<std::mutex> lk(pending_mu_);
                auto it = pending_.find(cid);
                if (it != pending_.end()) {
                    handler = std::move(it->second);
                    pending_.erase(it);
                }
            }
            if (handler) handler(j);
            break;
        }
        case FrameKind::PushMessage: {
            const auto& rk = desc.route_key;
            std::function<void(const json&)> cb;
            {
                std::lock_guard<std::mutex> lk(subs_mu_);
                auto it = subscriptions_.find(rk);
                if (it != subscriptions_.end()) cb = it->second;
            }
            if (cb) cb(j);
            break;
        }
        case FrameKind::Unknown:
            break;
    }
}

} // namespace exchange::ws
