// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/ws_api.hpp
// Binance WebSocket API (bidirectional trading) — request/response types,
// frame descriptor, and client factory for the ws-api endpoint.
//
// Namespace: exchange::binance::ws (sibling of ws_streams.hpp — the two
// surfaces are independently includable and co-includable).
//
// Built entirely on exchange::ws::ExchangeWsClient. Every frame on this
// endpoint is a method call correlated by "id" — there is no push/channel
// concept, so only the execute()/execute_async() half of the client is used.
// Wire shapes are documented in
// docs/plans/001-appendix-binance-message-formats.md §4/§5.
//
// This header does not include ix_ws_connection.hpp; for the real transport
// use the URL overload of exchange::ws::make_exchange_ws_client there.

#include "exchange/common/ws_client.hpp"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace exchange::binance::ws {

using json = nlohmann::json;

// ── Re-export common client types ────────────────────────────────────────────

using exchange::ws::IWsConnection;
using exchange::ws::IWsErrorHandler;
using exchange::ws::RateLimitedWsErrorHandler;
using exchange::ws::WsResponse;
using exchange::ws::ExchangeWsClient;
using exchange::ws::MessageIdentifier;
using exchange::ws::WsRequestBase;
using exchange::ws::TypedWsRequest;
using exchange::ws::FrameDescriptor;
using exchange::ws::FrameKind;
using exchange::ws::BaseWsResponse;

// ── Endpoint URL constant ─────────────────────────────────────────────────────

inline constexpr std::string_view WS_API_URL = "wss://ws-api.binance.com/ws-api/v3";

// ── Reply envelope ────────────────────────────────────────────────────────────
//
// Every WS API reply shares the shell
//   {"id":…,"status":N,"result":{…}|"error":{"code":C,"msg":M},"rateLimits":[…]}
// (appendix §4). BinanceWsApiResponse carries the shell; each concrete
// response type derives it and adds its typed result. Deriving BaseWsResponse
// lets the generic detail::make_ws_response derive WsResponse::ok from
// success/error with no client change — the same type-dispatch Kraken's
// BaseResponse uses.

// One entry of the "rateLimits" array — the API's flow-control feedback
// (ORDERS budget consumed) returned on every trading reply. Plain JSON
// numbers, not string-encoded.
struct BinanceWsRateLimit {
    std::string rate_limit_type;   // "ORDERS", "REQUEST_WEIGHT", …
    std::string interval;          // "SECOND", "MINUTE", "DAY"
    int         interval_num{0};
    int64_t     limit{0};
    int64_t     count{0};

    static BinanceWsRateLimit from_json(const json& j) {
        BinanceWsRateLimit r;
        r.rate_limit_type = j.value("rateLimitType", "");
        r.interval        = j.value("interval", "");
        r.interval_num    = j.value("intervalNum", 0);
        r.limit           = j.value("limit", int64_t{0});
        r.count           = j.value("count", int64_t{0});
        return r;
    }
};

struct BinanceWsApiResponse : exchange::ws::BaseWsResponse {
    int                             status{0};       // mirrors HTTP status codes
    std::optional<int64_t>          id;              // echoed request id (int form)
    std::optional<int>              error_code;      // error.code, e.g. -2010
    std::vector<BinanceWsRateLimit> rate_limits;
};

namespace detail {

// Parses the shared reply shell into the envelope base. success requires a
// 2xx/3xx status *and* no error object — for well-formed frames the two
// always agree; requiring both keeps a malformed frame (error object with a
// missing status) from reading as success.
inline void parse_ws_api_envelope(BinanceWsApiResponse& r, const json& j) {
    r.status = j.value("status", 0);

    // We always emit int64 ids; tolerate (skip) the string form a non-library
    // producer might use — correlation has already happened in the descriptor.
    if (j.contains("id") && j.at("id").is_number_integer())
        r.id = j.at("id").get<int64_t>();

    const bool has_error = j.contains("error") && !j.at("error").is_null();
    if (has_error) {
        r.error      = j.at("error").value("msg", "");
        r.error_code = j.at("error").value("code", 0);
    }
    r.success = !has_error && r.status < 400;

    for (const auto& el : j.value("rateLimits", json::array()))
        r.rate_limits.push_back(BinanceWsRateLimit::from_json(el));
}

} // namespace detail

// ── Frame descriptor (MessageIdentifier) ──────────────────────────────────────

// Classifies ws-api frames per appendix §5: any frame with a non-null "id" is
// a MethodResponse; there is no push branch on this endpoint. String ids pass
// through verbatim; integer ids stringify exactly like the client's
// std::to_string(req_id) pending key (the same contract the stream descriptor
// pins). id:null (malformed-request reply) is uncorrelatable → Unknown.
inline exchange::ws::FrameDescriptor
binance_ws_api_frame_descriptor(const json& j) {
    exchange::ws::FrameDescriptor d;

    if (j.contains("id") && !j.at("id").is_null()) {
        const auto& id   = j.at("id");
        d.kind           = FrameKind::MethodResponse;
        d.correlation_id = id.is_string() ? id.get<std::string>()
                                          : std::to_string(id.get<int64_t>());
    }

    return d;  // FrameKind::Unknown when no usable id
}

// ── ping ──────────────────────────────────────────────────────────────────────

// Reply is envelope-only: {"id":N,"status":200,"result":{}}.
struct BinanceWsPongMessage : BinanceWsApiResponse {
    static BinanceWsPongMessage from_json(const json& j) {
        BinanceWsPongMessage m;
        detail::parse_ws_api_envelope(m, j);
        return m;
    }
};

// Unsigned heartbeat / connectivity probe — the endpoint's only
// credential-free method in scope.
struct BinanceWsPingRequest : TypedWsRequest<BinanceWsPongMessage> {
    json to_json() const {
        return {{"id", req_id}, {"method", "ping"}};
    }
};

} // namespace exchange::binance::ws
