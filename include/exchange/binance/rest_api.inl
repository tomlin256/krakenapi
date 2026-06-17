// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/binance/rest_api.inl — template implementations for rest_api.hpp.
// Included at the bottom of rest_api.hpp; do not include directly.

#pragma once

#include <string>

namespace exchange::binance::rest {

// Binance has no wrapping envelope — success response is the bare object/array.
// Error shape: {"code": <negative_int>, "msg": "<description>"}
// Success signal: HTTP 2xx AND no "code" field in the response body.
template<typename T>
exchange::rest::RestResponse<T>
parse_binance_response(int http_status, const json& j) {
    exchange::rest::RestResponse<T> resp;
    if (http_status < 400 && !(j.is_object() && j.contains("code"))) {
        resp.ok     = true;
        resp.result = T::from_json(j);
    } else {
        resp.ok = false;
        std::string msg;
        if (j.is_object() && j.contains("msg"))
            msg = j.value("msg", "unknown error");
        else
            msg = "HTTP " + std::to_string(http_status);
        resp.errors.push_back(std::move(msg));
    }
    return resp;
}

} // namespace exchange::binance::rest
