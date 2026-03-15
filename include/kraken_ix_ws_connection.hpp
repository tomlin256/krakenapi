// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_ix_ws_connection.hpp
// ixwebsocket concrete implementation of IWsConnection, plus the URL-based
// factory overload of make_ws_client().
//
// Include this header (in addition to kraken_ws_client.hpp) when you want to
// use the real ixwebsocket transport.  Unit tests that use a mock connection
// should include only kraken_ws_client.hpp to avoid pulling in ixwebsocket.
//
// Usage:
//   #include "kraken_ix_ws_connection.hpp"
//   auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

#include "kraken_ws_client.hpp"

#include <ixwebsocket/IXWebSocket.h>

namespace kraken::ws {

// ─────────────────────────────────────────────────────────────────────────────
// IxWsConnection  –  ixwebsocket implementation of IWsConnection
// ─────────────────────────────────────────────────────────────────────────────

class IxWsConnection : public IWsConnection {
public:
    explicit IxWsConnection(std::string url) : url_(std::move(url)) {}

    void connect() override {
        ws_.setUrl(url_);
        ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
            switch (msg->type) {
                case ix::WebSocketMessageType::Open:
                    if (open_cb_)  open_cb_();
                    break;
                case ix::WebSocketMessageType::Close:
                    if (close_cb_) close_cb_();
                    break;
                case ix::WebSocketMessageType::Message:
                    if (msg_cb_)   msg_cb_(msg->str);
                    break;
                default:
                    break;
            }
        });
        ws_.start();
    }

    void disconnect() override { ws_.stop(); }

    bool is_connected() const override {
        return ws_.getReadyState() == ix::ReadyState::Open;
    }

    void send(const std::string& msg) override { ws_.send(msg); }

    void set_on_message(MessageCb cb) override { msg_cb_  = std::move(cb); }
    void set_on_open(OpenCb cb)       override { open_cb_ = std::move(cb); }
    void set_on_close(CloseCb cb)     override { close_cb_= std::move(cb); }

private:
    std::string   url_;
    ix::WebSocket ws_;
    MessageCb     msg_cb_;
    OpenCb        open_cb_;
    CloseCb       close_cb_;
};

// ─────────────────────────────────────────────────────────────────────────────
// URL-based factory: creates a fresh IxWsConnection and starts connecting.
// Phase 1 (on_open) fires asynchronously on a background thread; any
// subscribe/execute calls made before it fires are queued internally.
// ─────────────────────────────────────────────────────────────────────────────

inline std::shared_ptr<KrakenWsClient>
make_ws_client(const std::string&               url,
               std::shared_ptr<IWsErrorHandler>  error_handler = nullptr) {
    auto conn   = std::make_shared<IxWsConnection>(url);
    auto client = std::make_shared<KrakenWsClient>(conn, std::move(error_handler));
    client->init();
    conn->connect();
    return client;
}

} // namespace kraken::ws
