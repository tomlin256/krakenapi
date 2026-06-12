// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/ws_streams.hpp
// Binance WebSocket market streams — push event types, subscribe scaffold,
// frame descriptor, and client factory for the combined-stream endpoint.
//
// Namespace: exchange::binance::ws
//
// Built entirely on exchange::ws::ExchangeWsClient — Binance contributes only
// a MessageIdentifier (binance_stream_frame_descriptor), event payload types,
// the SUBSCRIBE/UNSUBSCRIBE request scaffold, and a factory. Wire shapes are
// documented in docs/plans/001-appendix-binance-message-formats.md §3/§5.
//
// This header does not include ix_ws_connection.hpp; for the real transport
// use the URL overload of exchange::ws::make_exchange_ws_client there.

#include "exchange/common/ws_client.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace exchange::binance::ws {

using json = nlohmann::json;

// ── Re-export common client types ────────────────────────────────────────────

using exchange::ws::IWsConnection;
using exchange::ws::IWsErrorHandler;
using exchange::ws::RateLimitedWsErrorHandler;
using exchange::ws::WsResponse;
using exchange::ws::SubscriptionHandle;
using exchange::ws::ExchangeWsClient;
using exchange::ws::MessageIdentifier;
using exchange::ws::WsRequestBase;
using exchange::ws::FrameDescriptor;
using exchange::ws::FrameKind;
using exchange::ws::BaseWsResponse;

// ── Endpoint URL constant ─────────────────────────────────────────────────────

// Combined endpoint — every push frame arrives wrapped as
// {"stream":"<name>","data":{…}}, which is what the frame descriptor routes by.
inline constexpr std::string_view STREAM_URL = "wss://stream.binance.com/stream";

// ── Payload unwrapping ────────────────────────────────────────────────────────

namespace detail {

// ExchangeWsClient hands the *whole* parsed frame to push callbacks, so on the
// combined endpoint every event arrives as {"stream":…,"data":{…}}. Bare
// payloads (documented fixtures, raw /ws single-stream use) have no wrapper.
// Every event from_json parses through this so both shapes are accepted.
//
// Returns a reference into the argument to avoid copying the payload subtree
// on every push frame. Lifetime contract: the frame must outlive the returned
// reference — every caller is an event from_json(const json&) using the result
// within its own body, where the frame is dispatch's lvalue.
inline const json& stream_payload(const json& j) {
    // NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter)
    return j.contains("data") ? j.at("data") : j;
}

} // namespace detail

// ── SUBSCRIBE/UNSUBSCRIBE ack ─────────────────────────────────────────────────

// Success: {"result":null,"id":N}. Failure: {"error":{"code":C,"msg":M},"id":N}.
// Deriving BaseWsResponse lets the generic detail::make_ws_response derive
// WsResponse::ok from success/error — no client change needed.
struct BinanceStreamAck : exchange::ws::BaseWsResponse {
    int64_t id{0};

    static BinanceStreamAck from_json(const json& j) {
        BinanceStreamAck a;
        if (j.contains("id") && !j.at("id").is_null())
            a.id = j.at("id").get<int64_t>();
        if (j.contains("error") && !j.at("error").is_null()) {
            a.success = false;
            a.error   = j.at("error").value("msg", "");
        } else {
            a.success = true;
        }
        return a;
    }
};

// ── Frame descriptor (MessageIdentifier) ──────────────────────────────────────

// Classifies combined-endpoint frames per appendix §5:
//   {"stream":"X@s","data":{…}}            → PushMessage, route_key = "X@s"
//   {"result"/"error":…, "id":N (non-null)} → MethodResponse, correlation_id = str(N)
//   anything else (incl. id:null)           → Unknown
//
// ExchangeWsClient keys pending handlers by std::to_string(req_id), so the
// correlation id here must stringify the integer id the same way.
inline exchange::ws::FrameDescriptor
binance_stream_frame_descriptor(const json& j) {
    exchange::ws::FrameDescriptor d;

    if (j.contains("stream")) {
        d.kind      = FrameKind::PushMessage;
        d.route_key = j.at("stream").get<std::string>();
        return d;
    }

    if (j.contains("id") && !j.at("id").is_null()
        && (j.contains("result") || j.contains("error"))) {
        const auto& id   = j.at("id");
        d.kind           = FrameKind::MethodResponse;
        d.correlation_id = id.is_string() ? id.get<std::string>()
                                          : std::to_string(id.get<int64_t>());
        return d;
    }

    return d;  // FrameKind::Unknown
}

} // namespace exchange::binance::ws
