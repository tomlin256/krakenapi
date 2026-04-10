// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// ws_reconnect_session.hpp — transport-level WebSocket reconnect machinery.
//
// WsReconnectSession encapsulates the background thread, mutex/cv, exponential
// backoff, and stop signal that every reconnecting WS consumer needs. Callers
// supply two callbacks and never implement the thread machinery themselves:
//
//   ConnectFn    — called to open (or re-open) the connection; throws on failure.
//   DisconnectFn — called to tear down the current connection before reconnecting,
//                  and once more on stop(). Must not throw.
//
// Typical usage:
//
//   WsReconnectSession session(
//       [&]{ conn.connect(); conn.subscribe(); },   // ConnectFn
//       [&]{ conn.disconnect(); }                   // DisconnectFn
//   );
//   session.start();
//
//   // In a WS disconnect handler (any thread):
//   conn.set_on_close([&](std::string reason) {
//       session.scheduleReconnect(std::move(reason));
//   });
//
//   // On shutdown:
//   session.stop();
//
// Backoff policy: initial = minBackoff (default 1 s), doubles per consecutive
// failure, caps at 60 s. Resets to minBackoff after a successful connect.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace kraken::ws {

class WsReconnectSession {
public:
    using ConnectFn    = std::function<void()>;  // throws on failure
    using DisconnectFn = std::function<void()>;  // must not throw

    static constexpr std::chrono::milliseconds kMaxBackoff{60000};

    // Construct with connect and disconnect callbacks.
    // minBackoff: initial backoff duration before the first reconnect attempt.
    WsReconnectSession(ConnectFn    connect,
                       DisconnectFn disconnect,
                       std::chrono::milliseconds minBackoff = std::chrono::seconds{1});

    ~WsReconnectSession();

    // Establish the initial connection and start the background reconnect thread.
    // Calls connect_fn_() on the calling thread; throws if the initial connect fails.
    // Must be called at most once.
    void start();

    // Signal the reconnect thread to stop, wait for it to exit, then call
    // disconnect_fn_() to tear down the live connection.
    // Safe to call even if start() was never called or threw.
    void stop();

    // Schedule a reconnect attempt. Safe to call from any thread, including a
    // WS disconnect handler. No-op if stop() has already been requested.
    void scheduleReconnect(std::string reason = "");

private:
    ConnectFn                 connect_fn_;
    DisconnectFn              disconnect_fn_;
    std::chrono::milliseconds min_backoff_;

    std::atomic<bool>       stop_requested_{false};
    std::atomic<bool>       reconnect_requested_{false};
    std::mutex              mu_;
    std::condition_variable cv_;
    std::thread             worker_;

    void workerLoop();
};

} // namespace kraken::ws

#include "ws_reconnect_session.inl"
