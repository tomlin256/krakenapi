// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/binance/rest_client.inl — template method implementations for
// BinanceRestClient. Included at the bottom of rest_client.hpp; do not include
// directly.

#pragma once

namespace exchange::binance::rest {

// Execute a public request (no credentials required).
template<typename Req, typename Enable>
exchange::rest::RestResponse<typename Req::response_type>
BinanceRestClient::execute(const Req& req) {
    using Resp = typename Req::response_type;
    try {
        auto http = req.build();
        const HttpResponse r = perform_(http);
        return parse_binance_response<Resp>(r.status, json::parse(r.body));
    } catch (const std::exception& e) {
        // Transport / malformed-body / signing failures fold into the
        // envelope so `resp.ok` is the single failure check (review M2).
        exchange::rest::RestResponse<Resp> err;
        err.errors.push_back(std::string("request failed: ") + e.what());
        return err;
    }
}

// Execute a private request (BinanceAuth injects timestamp + signature).
template<typename Req, typename Enable>
exchange::rest::RestResponse<typename Req::response_type>
BinanceRestClient::execute(const Req& req, const BinanceAuth& auth) {
    using Resp = typename Req::response_type;
    try {
        auto http = req.build();
        auth.sign(http);
        const HttpResponse r = perform_(http);
        return parse_binance_response<Resp>(r.status, json::parse(r.body));
    } catch (const std::exception& e) {
        // Transport / malformed-body / signing failures (incl. a rejected
        // non-HMAC algorithm) fold into the envelope (review M2).
        exchange::rest::RestResponse<Resp> err;
        err.errors.push_back(std::string("request failed: ") + e.what());
        return err;
    }
}

} // namespace exchange::binance::rest
