// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/kraken/rest_client.inl — template method implementations for
// KrakenRestClient. Included at the bottom of rest_client.hpp; do not include
// directly.

#pragma once

namespace exchange::kraken::rest {

// Execute a public request (no credentials required).
template<typename Req, typename Enable>
exchange::kraken::RestResponse<typename Req::response_type>
KrakenRestClient::execute(const Req& req) {
    using Resp = typename Req::response_type;
    try {
        auto http = req.build();
        const HttpResponse r = perform_(http);
        return exchange::kraken::parse_rest_response<Resp>(json::parse(r.body));
    } catch (const std::exception& e) {
        // Transport / malformed-body failures fold into the envelope so
        // `resp.ok` is the single failure check (review M2).
        exchange::kraken::RestResponse<Resp> err;
        err.errors.push_back(std::string("request failed: ") + e.what());
        return err;
    }
}

// Execute a private request (credentials required).
template<typename Req, typename Enable>
exchange::kraken::RestResponse<typename Req::response_type>
KrakenRestClient::execute(const Req& req, const KrakenCredentials& creds) {
    using Resp = typename Req::response_type;
    try {
        auto http = req.build(creds);
        const HttpResponse r = perform_(http);
        return exchange::kraken::parse_rest_response<Resp>(json::parse(r.body));
    } catch (const std::exception& e) {
        exchange::kraken::RestResponse<Resp> err;
        err.errors.push_back(std::string("request failed: ") + e.what());
        return err;
    }
}

} // namespace exchange::kraken::rest
