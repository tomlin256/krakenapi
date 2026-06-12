// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// MockWsConnection — shared test double for ExchangeWsClient suites (Kraken
// and Binance). Avoids any real network I/O; tests can:
//   - Inspect outbound messages (conn->sent_messages)
//   - Inject inbound server frames (conn->inject_message(...))
//   - Simulate connection open/close (conn->fire_open() / fire_close())

#pragma once

#include "exchange/common/ws.hpp"

#include <string>
#include <utility>
#include <vector>

class MockWsConnection : public exchange::ws::IWsConnection {
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
    void set_on_error(ErrorCb cb)     override { error_cb_= std::move(cb); }

    // Test helpers
    void inject_message(const std::string& raw)  { if (msg_cb_)   msg_cb_(raw);    }
    void fire_open()                              { if (open_cb_)  open_cb_();      }
    void fire_close(std::string reason = "")      { if (close_cb_) close_cb_(std::move(reason)); }
    void fire_error(const std::string& reason)    { if (error_cb_) error_cb_(reason); }

private:
    bool      connected_{false};
    MessageCb msg_cb_;
    OpenCb    open_cb_;
    CloseCb   close_cb_;
    ErrorCb   error_cb_;
};
