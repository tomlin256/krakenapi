// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/cryptocom/heartbeat_connection.hpp
// HeartbeatResponder — an IWsConnection decorator that answers Crypto.com's
// mandatory keepalive. Crypto.com sends {"method":"public/heartbeat","id":N}
// and disconnects clients that do not reply {"method":"public/respond-heartbeat",
// "id":N}. The generic ExchangeWsClient has no concept of replying to an inbound
// frame, so this small adapter-local decorator wraps the real connection,
// auto-answers heartbeats (and swallows them), and forwards every other frame
// untouched. This keeps exchange_common unmodified (plan 020 §2, Option A).
//
// Bodies are defined in src/cryptocom/heartbeat_connection.cpp.
//
// Namespace: exchange::cryptocom::ws

#include "exchange/common/ws.hpp"

#include <memory>
#include <string>

namespace exchange::cryptocom::ws {

using exchange::ws::IWsConnection;

class HeartbeatResponder : public IWsConnection {
public:
    explicit HeartbeatResponder(std::shared_ptr<IWsConnection> inner);

    // Transport methods forward to the wrapped connection.
    void connect() override;
    void disconnect() override;
    bool is_connected() const override;
    void send(const std::string& msg) override;

    // Installs an intercepting handler on the inner connection: a
    // "public/heartbeat" frame is answered + swallowed; everything else is
    // forwarded to cb. The other callbacks forward straight through.
    void set_on_message(MessageCb cb) override;
    void set_on_open(OpenCb cb) override;
    void set_on_close(CloseCb cb) override;
    void set_on_error(ErrorCb cb) override;

private:
    void handle_inbound(const std::string& raw);

    std::shared_ptr<IWsConnection> inner_;
    MessageCb                      forward_;
};

} // namespace exchange::cryptocom::ws
