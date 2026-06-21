// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/cryptocom/rest_client.inl — template method implementations for
// CryptoComRestClient. Included at the bottom of rest_client.hpp; do not include
// directly.

#pragma once

namespace exchange::cryptocom::rest {

// Shared perform + parse: transport or malformed-body failures fold into the
// envelope so `resp.ok` is the single failure check (mirrors the other adapters).
template<typename R>
exchange::rest::RestResponse<R> CryptoComRestClient::dispatch(const HttpRequest& http) {
    try {
        const HttpResponse r = perform_(http);
        return parse_cryptocom_response<R>(r.status, json::parse(r.body));
    } catch (const std::exception& e) {
        exchange::rest::RestResponse<R> err;
        err.errors.push_back(std::string("request failed: ") + e.what());
        return err;
    }
}

// Execute a public request (no credentials required).
template<typename Req, typename Enable>
exchange::rest::RestResponse<typename Req::response_type>
CryptoComRestClient::execute(const Req& req) {
    return dispatch<typename Req::response_type>(req.build());
}

// Execute a private request — assign a unique correlation id, then let the
// request build + sign its own envelope.
template<typename Req, typename Enable>
exchange::rest::RestResponse<typename Req::response_type>
CryptoComRestClient::execute(const Req& req, const CryptoComCredentials& creds) {
    Req signed_req = req;
    signed_req.id  = next_id_++;
    return dispatch<typename Req::response_type>(signed_req.build(creds));
}

} // namespace exchange::cryptocom::rest
