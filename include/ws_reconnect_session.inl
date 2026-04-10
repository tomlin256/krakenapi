// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// ws_reconnect_session.inl — WsReconnectSession method implementations.
// Included at the bottom of ws_reconnect_session.hpp; never include directly.

namespace kraken::ws {

inline WsReconnectSession::WsReconnectSession(ConnectFn    connect,
                                               DisconnectFn disconnect,
                                               std::chrono::milliseconds minBackoff)
    : connect_fn_(std::move(connect))
    , disconnect_fn_(std::move(disconnect))
    , min_backoff_(minBackoff)
{}

inline WsReconnectSession::~WsReconnectSession() {
    stop();
}

inline void WsReconnectSession::start() {
    connect_fn_();  // throws on failure; let it propagate to caller
    worker_ = std::thread([this]{ workerLoop(); });
}

inline void WsReconnectSession::stop() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        stop_requested_.store(true, std::memory_order_relaxed);
    }
    cv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }

    // Tear down the live connection. Exceptions must not propagate from
    // disconnect_fn_ (contract), but guard anyway.
    try { disconnect_fn_(); } catch (...) {}
}

inline void WsReconnectSession::scheduleReconnect(std::string /*reason*/) {
    if (stop_requested_.load(std::memory_order_relaxed)) return;
    {
        std::lock_guard<std::mutex> lk(mu_);
        reconnect_requested_.store(true, std::memory_order_relaxed);
    }
    cv_.notify_all();
}

inline void WsReconnectSession::workerLoop() {
    while (true) {
        // ── Wait for a reconnect request or stop ─────────────────────────────
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [this] {
                return stop_requested_.load(std::memory_order_relaxed)
                    || reconnect_requested_.load(std::memory_order_relaxed);
            });

            if (stop_requested_.load(std::memory_order_relaxed)) return;
            reconnect_requested_.store(false, std::memory_order_relaxed);
        }

        // ── Reconnect loop with exponential backoff ───────────────────────────
        auto backoff = min_backoff_;

        while (true) {
            // Wait for the backoff duration, but exit early if stop fires.
            {
                std::unique_lock<std::mutex> lk(mu_);
                cv_.wait_for(lk, backoff, [this] {
                    return stop_requested_.load(std::memory_order_relaxed);
                });
                if (stop_requested_.load(std::memory_order_relaxed)) return;
            }

            // Tear down the stale connection, then attempt to reconnect.
            try { disconnect_fn_(); } catch (...) {}

            try {
                connect_fn_();
                break;  // success — return to outer wait loop
            } catch (...) {
                // Failure — double the backoff and retry.
                backoff = std::min(backoff * 2, kMaxBackoff);
            }
        }
    }
}

} // namespace kraken::ws
