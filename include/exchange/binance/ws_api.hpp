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

#include "exchange/binance/auth.hpp"
#include "exchange/binance/rest_api.hpp"
#include "exchange/common/ws_client.hpp"

#include <chrono>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
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

// ── Signing ───────────────────────────────────────────────────────────────────
//
// The WS API uses the same key material, algorithm (HMAC-SHA256 → lowercase
// hex), and recvWindow semantics as REST — only the payload framing differs
// (alphabetically-sorted "k=v&k=v" over the params object, vs. REST's
// query+body concatenation). So the credential bundle is the REST one by
// alias, and signing is one free helper over the same crypto primitives.

using BinanceWsCredentials = exchange::binance::rest::BinanceCredentials;

namespace detail {

// Renders one param value as it appears in the signed payload: strings raw
// (no quotes), everything else via dump() (ints → digits, bools → true/false).
inline std::string ws_param_value(const json& v) {
    return v.is_string() ? v.get<std::string>() : v.dump();
}

// Injects apiKey, timestamp, and recvWindow (if creds.recv_window_ms > 0)
// into params, then appends "signature" — HMAC-SHA256 hex over the sorted
// payload. nlohmann's json object is std::map-backed, so key iteration is
// already alphabetical: Binance's required signing order falls out with no
// sort step. Payload and wire frame are rendered from the same params
// object, so they cannot disagree. The request "id" lives outside params and
// is never part of the signature. HMAC-SHA256 only, like BinanceAuth::sign
// (Rsa/Ed25519 enum values are accepted-but-unimplemented there too).
inline void ws_sign_params(json& params, const BinanceWsCredentials& creds,
                           int64_t timestamp_ms) {
    // HMAC-SHA256 only — reject other algorithms loudly (see BinanceAuth::sign).
    if (creds.algorithm != rest::BinanceSignAlgorithm::HmacSha256)
        throw std::invalid_argument(
            "Binance WS signing: only HMAC-SHA256 is implemented "
            "(RSA / Ed25519 are not supported)");

    params["apiKey"]    = creds.api_key;
    params["timestamp"] = timestamp_ms;
    if (creds.recv_window_ms > 0)
        params["recvWindow"] = creds.recv_window_ms;

    std::string payload;
    for (auto it = params.begin(); it != params.end(); ++it) {
        if (!payload.empty()) payload += '&';
        payload += it.key() + '=' + ws_param_value(it.value());
    }

    params["signature"] =
        rest::detail::to_hex(rest::detail::hmac_sha256(creds.secret_key, payload));
}

// Current wall-clock ms — the default when a request's timestamp is unset.
inline int64_t ws_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace detail

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

// ── order.place / order.cancel ────────────────────────────────────────────────
//
// Field sets mirror the REST request structs (same wire names, same
// caller-formatted-decimal-string convention, same enum converters); result
// payloads are the REST response shapes verbatim, so the WS responses embed
// them rather than duplicating the parsers (plan 006 decision 5). Signed
// requests carry creds plus an explicit timestamp: 0 means "system clock at
// to_json() time" (live callers do nothing); tests set a fixed value, making
// the whole frame — signature included — exactly assertable. ExchangeWsClient
// assigns req_id and then calls to_json() once, so signing happens there; the
// id is outside params and never enters the signature.

struct BinanceWsNewOrderResponse : BinanceWsApiResponse {
    // Set on success; parses like the REST RESULT/FULL order shape.
    std::optional<exchange::binance::rest::BinanceNewOrderResponse> order;

    static BinanceWsNewOrderResponse from_json(const json& j) {
        BinanceWsNewOrderResponse r;
        detail::parse_ws_api_envelope(r, j);
        if (j.contains("result") && j.at("result").is_object())
            r.order = rest::BinanceNewOrderResponse::from_json(j.at("result"));
        return r;
    }
};

struct BinanceWsNewOrderRequest : TypedWsRequest<BinanceWsNewOrderResponse> {
    BinanceWsCredentials creds;
    int64_t              timestamp{0};  // ms; 0 → now at to_json() time

    std::string symbol;                  // required
    Side        side{Side::Buy};         // required
    OrderType   type{OrderType::Limit};  // required
    std::optional<TimeInForce>          time_in_force;
    std::optional<std::string>          quantity;          // caller-formatted exact
    std::optional<std::string>          quote_order_qty;   //   decimals (plan 004
    std::optional<std::string>          price;             //   decision 6)
    std::optional<std::string>          new_client_order_id;
    std::optional<std::string>          stop_price;
    std::optional<std::string>          iceberg_qty;
    std::optional<BinanceOrderRespType> new_order_resp_type;

    json to_json() const {
        json params{
            {"symbol", symbol},
            {"side", exchange::binance::binance_side_to_string(side)},
            {"type", exchange::binance::binance_order_type_to_string(type)},
        };
        if (time_in_force)
            params["timeInForce"] = exchange::binance::binance_tif_to_string(*time_in_force);
        if (quantity)            params["quantity"] = *quantity;
        if (quote_order_qty)     params["quoteOrderQty"] = *quote_order_qty;
        if (price)               params["price"] = *price;
        if (new_client_order_id) params["newClientOrderId"] = *new_client_order_id;
        if (stop_price)          params["stopPrice"] = *stop_price;
        if (iceberg_qty)         params["icebergQty"] = *iceberg_qty;
        if (new_order_resp_type)
            params["newOrderRespType"] =
                exchange::binance::binance_order_resp_type_to_string(*new_order_resp_type);

        detail::ws_sign_params(params, creds,
                               timestamp != 0 ? timestamp : detail::ws_now_ms());
        return {{"id", req_id}, {"method", "order.place"}, {"params", std::move(params)}};
    }
};

struct BinanceWsCancelOrderResponse : BinanceWsApiResponse {
    // Set on success; the DELETE /api/v3/order shape inside the §4 envelope.
    std::optional<exchange::binance::rest::BinanceCancelOrderResponse> order;

    static BinanceWsCancelOrderResponse from_json(const json& j) {
        BinanceWsCancelOrderResponse r;
        detail::parse_ws_api_envelope(r, j);
        if (j.contains("result") && j.at("result").is_object())
            r.order = rest::BinanceCancelOrderResponse::from_json(j.at("result"));
        return r;
    }
};

struct BinanceWsCancelOrderRequest : TypedWsRequest<BinanceWsCancelOrderResponse> {
    BinanceWsCredentials creds;
    int64_t              timestamp{0};  // ms; 0 → now at to_json() time

    std::string                symbol;                // required
    std::optional<int64_t>     order_id;              // one of order_id /
    std::optional<std::string> orig_client_order_id;  //   orig_client_order_id
    std::optional<std::string> new_client_order_id;

    json to_json() const {
        json params{{"symbol", symbol}};
        if (order_id)             params["orderId"] = *order_id;
        if (orig_client_order_id) params["origClientOrderId"] = *orig_client_order_id;
        if (new_client_order_id)  params["newClientOrderId"] = *new_client_order_id;

        detail::ws_sign_params(params, creds,
                               timestamp != 0 ? timestamp : detail::ws_now_ms());
        return {{"id", req_id}, {"method", "order.cancel"}, {"params", std::move(params)}};
    }
};

// ── Client alias and connection-based factory ─────────────────────────────────
//
// BinanceWsApiClient is ExchangeWsClient parameterised with the WS API frame
// descriptor — the same runtime type as BinanceStreamClient / KrakenWsClient.
// For the real transport use the URL overload in
// exchange/common/ix_ws_connection.hpp:
//   exchange::ws::make_exchange_ws_client(std::string(WS_API_URL),
//                                         binance_ws_api_frame_descriptor);

using BinanceWsApiClient = exchange::ws::ExchangeWsClient;

inline std::shared_ptr<BinanceWsApiClient>
make_binance_ws_api_client(std::shared_ptr<IWsConnection>  conn,
                           std::shared_ptr<IWsErrorHandler> error_handler = nullptr) {
    return exchange::ws::make_exchange_ws_client(
        std::move(conn),
        binance_ws_api_frame_descriptor,
        std::move(error_handler));
}

} // namespace exchange::binance::ws
