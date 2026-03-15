// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "kraken_ws_client.hpp"

namespace kraken::ws {

// ─────────────────────────────────────────────────────────────────────────────
// SubscriptionHandle
// ─────────────────────────────────────────────────────────────────────────────

void SubscriptionHandle::cancel() {
    if (!active_ || !active_->exchange(false))
        return;  // already cancelled or default-constructed (inactive)
    if (auto c = client_.lock())
        c->cancel_subscription(channel_, unsub_json_);
}

// ─────────────────────────────────────────────────────────────────────────────
// KrakenWsClient
// ─────────────────────────────────────────────────────────────────────────────

void KrakenWsClient::init() {
    if (!error_handler_)
        error_handler_ = std::make_shared<RateLimitedWsErrorHandler>();

    if (conn_->is_connected())
        connected_.store(true);

    auto weak_self = std::weak_ptr<KrakenWsClient>(shared_from_this());

    conn_->set_on_message([weak_self](const std::string& raw) {
        if (auto self = weak_self.lock()) self->on_raw_message(raw);
    });
    conn_->set_on_open([weak_self]() {
        if (auto self = weak_self.lock()) self->on_open_handler();
    });
    conn_->set_on_close([weak_self]() {
        if (auto self = weak_self.lock()) self->connected_.store(false);
    });
}

void KrakenWsClient::cancel_subscription(const std::string& channel,
                                          const std::string& unsub_json) {
    {
        std::lock_guard<std::mutex> lk(subs_mu_);
        subscriptions_.erase(channel);
    }
    enqueue_or_send(unsub_json);
}

void KrakenWsClient::enqueue_or_send(const std::string& msg) {
    std::lock_guard<std::mutex> lk(queue_mu_);
    if (connected_.load()) {
        conn_->send(msg);
    } else {
        send_queue_.push_back(msg);
    }
}

void KrakenWsClient::on_open_handler() {
    std::vector<std::string> queued;
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        connected_.store(true);
        queued = std::move(send_queue_);
    }
    for (const auto& msg : queued)
        conn_->send(msg);
}

void KrakenWsClient::on_raw_message(const std::string& raw) {
    json j;
    try { j = json::parse(raw); }
    catch (const std::exception& e) { error_handler_->on_malformed_frame(raw, e); return; }

    // Method responses and subscribe/unsubscribe acks carry req_id.
    if (j.contains("req_id") && j["req_id"].is_number_integer()) {
        const auto id = j["req_id"].get<int64_t>();
        std::function<void(const json&)> handler;
        {
            std::lock_guard<std::mutex> lk(pending_mu_);
            auto it = pending_.find(id);
            if (it != pending_.end()) {
                handler = std::move(it->second);
                pending_.erase(it);
            }
        }
        if (handler) { handler(j); return; }
    }

    // Push channel frames (no req_id; identified by "channel" field).
    if (j.contains("channel") && j["channel"].is_string()) {
        const auto ch = j["channel"].get<std::string>();
        std::function<void(const json&)> cb;
        {
            std::lock_guard<std::mutex> lk(subs_mu_);
            auto it = subscriptions_.find(ch);
            if (it != subscriptions_.end()) cb = it->second;
        }
        if (cb) cb(j);
    }
}

} // namespace kraken::ws
