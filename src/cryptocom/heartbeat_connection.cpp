// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/cryptocom/heartbeat_connection.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace exchange::cryptocom::ws {

using json = nlohmann::json;

HeartbeatResponder::HeartbeatResponder(std::shared_ptr<IWsConnection> inner)
    : inner_(std::move(inner))
{}

void HeartbeatResponder::connect()              { inner_->connect(); }
void HeartbeatResponder::disconnect()           { inner_->disconnect(); }
bool HeartbeatResponder::is_connected() const   { return inner_->is_connected(); }
void HeartbeatResponder::send(const std::string& msg) { inner_->send(msg); }

void HeartbeatResponder::set_on_message(MessageCb cb) {
    forward_ = std::move(cb);
    inner_->set_on_message([this](const std::string& raw) { handle_inbound(raw); });
}

void HeartbeatResponder::set_on_open(OpenCb cb)   { inner_->set_on_open(std::move(cb)); }
void HeartbeatResponder::set_on_close(CloseCb cb) { inner_->set_on_close(std::move(cb)); }
void HeartbeatResponder::set_on_error(ErrorCb cb) { inner_->set_on_error(std::move(cb)); }

void HeartbeatResponder::handle_inbound(const std::string& raw) {
    // Answer + swallow heartbeats; forward everything else (including frames that
    // fail to parse — the client's own error handling deals with those).
    json j = json::parse(raw, nullptr, /*allow_exceptions=*/false);
    if (j.is_object() && j.value("method", std::string{}) == "public/heartbeat") {
        json reply;
        reply["id"]     = j.contains("id") ? j.at("id") : json(nullptr);
        reply["method"] = "public/respond-heartbeat";
        inner_->send(reply.dump());
        return;
    }
    if (forward_)
        forward_(raw);
}

} // namespace exchange::cryptocom::ws
