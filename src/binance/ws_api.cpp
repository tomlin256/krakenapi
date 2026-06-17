// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/ws_api.hpp"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace exchange::binance::ws {

// ── Reply envelope ────────────────────────────────────────────────────────────

BinanceWsRateLimit BinanceWsRateLimit::from_json(const json& j) {
    BinanceWsRateLimit r;
    r.rate_limit_type = j.value("rateLimitType", "");
    r.interval        = j.value("interval", "");
    r.interval_num    = j.value("intervalNum", 0);
    r.limit           = j.value("limit", int64_t{0});
    r.count           = j.value("count", int64_t{0});
    return r;
}

namespace detail {

void parse_ws_api_envelope(BinanceWsApiResponse& r, const json& j) {
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

exchange::ws::FrameDescriptor
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

namespace detail {

std::string ws_param_value(const json& v) {
    if (v.is_string()) return v.get<std::string>();
    // Floats are rejected (review L3): dump() may emit scientific notation, so a
    // float param would sign to a string the server never reconstructs. Numeric
    // params must be passed as caller-formatted decimal strings (price/qty) or
    // integers (orderId/timestamp); both round-trip exactly.
    if (v.is_number_float())
        throw std::invalid_argument(
            "ws_param_value: floating-point params must be caller-formatted "
            "decimal strings, not JSON numbers");
    return v.dump();
}

void ws_sign_params(json& params, const BinanceWsCredentials& creds,
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

int64_t ws_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

} // namespace detail

// ── ping ──────────────────────────────────────────────────────────────────────

BinanceWsPongMessage BinanceWsPongMessage::from_json(const json& j) {
    BinanceWsPongMessage m;
    detail::parse_ws_api_envelope(m, j);
    return m;
}

json BinanceWsPingRequest::to_json() const {
    return {{"id", req_id}, {"method", "ping"}};
}

// ── order.place / order.cancel ────────────────────────────────────────────────

BinanceWsNewOrderResponse BinanceWsNewOrderResponse::from_json(const json& j) {
    BinanceWsNewOrderResponse r;
    detail::parse_ws_api_envelope(r, j);
    if (j.contains("result") && j.at("result").is_object())
        r.order = rest::BinanceNewOrderResponse::from_json(j.at("result"));
    return r;
}

json BinanceWsNewOrderRequest::to_json() const {
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

BinanceWsCancelOrderResponse BinanceWsCancelOrderResponse::from_json(const json& j) {
    BinanceWsCancelOrderResponse r;
    detail::parse_ws_api_envelope(r, j);
    if (j.contains("result") && j.at("result").is_object())
        r.order = rest::BinanceCancelOrderResponse::from_json(j.at("result"));
    return r;
}

json BinanceWsCancelOrderRequest::to_json() const {
    json params{{"symbol", symbol}};
    if (order_id)             params["orderId"] = *order_id;
    if (orig_client_order_id) params["origClientOrderId"] = *orig_client_order_id;
    if (new_client_order_id)  params["newClientOrderId"] = *new_client_order_id;

    detail::ws_sign_params(params, creds,
                           timestamp != 0 ? timestamp : detail::ws_now_ms());
    return {{"id", req_id}, {"method", "order.cancel"}, {"params", std::move(params)}};
}

// ── Client factory ────────────────────────────────────────────────────────────

std::shared_ptr<BinanceWsApiClient>
make_binance_ws_api_client(std::shared_ptr<IWsConnection>  conn,
                           std::shared_ptr<IWsErrorHandler> error_handler) {
    return exchange::ws::make_exchange_ws_client(
        std::move(conn),
        binance_ws_api_frame_descriptor,
        std::move(error_handler));
}

} // namespace exchange::binance::ws
