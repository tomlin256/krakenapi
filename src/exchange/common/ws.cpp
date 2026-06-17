// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// Non-template definitions for the ws.hpp scaffold types. SubscriptionHandle and
// RateLimitedWsErrorHandler are declared in ws.hpp; their bodies live here (not
// in ws_client.inl) so the .inl stays template-only. cancel() needs the complete
// ExchangeWsClient, so this TU includes ws_client.hpp.

#include "exchange/common/ws_client.hpp"

#include <chrono>
#include <cstdio>

namespace exchange::ws {

// ── RateLimitedWsErrorHandler ────────────────────────────────────────────────

RateLimitedWsErrorHandler::RateLimitedWsErrorHandler(
    std::chrono::milliseconds interval)
    : interval_us_(std::chrono::duration_cast<std::chrono::microseconds>(interval).count())
{}

void RateLimitedWsErrorHandler::on_malformed_frame(
    const std::string& /*raw*/, const std::exception& e)
{
    const auto count  = ++count_;
    const auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto last   = last_logged_us_.load(std::memory_order_relaxed);

    if (count == 1 || now_us - last >= interval_us_) {
        last_logged_us_.store(now_us, std::memory_order_relaxed);
        std::fprintf(stderr,
            "ExchangeWsClient: failed to parse WebSocket frame"
            " (total malformed: %llu) — %s\n",
            static_cast<unsigned long long>(count), e.what());
    }
}

void RateLimitedWsErrorHandler::on_connection_error(const std::string& reason)
{
    std::fprintf(stderr,
        "ExchangeWsClient: WebSocket connection error — %s\n",
        reason.c_str());
}

// ── SubscriptionHandle ───────────────────────────────────────────────────────

bool SubscriptionHandle::is_active() const { return active_ && active_->load(); }

void SubscriptionHandle::cancel() {
    if (!active_ || !active_->exchange(false))
        return;
    if (auto c = client_.lock())
        c->cancel_subscription(route_key_, unsub_json_);
}

} // namespace exchange::ws
