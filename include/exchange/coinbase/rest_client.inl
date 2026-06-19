// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// exchange/coinbase/rest_client.inl — template method implementations for
// CoinbaseRestClient. Included at the bottom of rest_client.hpp; do not include
// directly.

#pragma once

namespace exchange::coinbase::rest {

// Execute a public request (no credentials required).
template<typename Req, typename Enable>
exchange::rest::RestResponse<typename Req::response_type>
CoinbaseRestClient::execute(const Req& req) {
    using Resp = typename Req::response_type;
    try {
        auto http = req.build();
        const HttpResponse r = perform_(http);
        return parse_coinbase_response<Resp>(r.status, json::parse(r.body));
    } catch (const std::exception& e) {
        // Transport / malformed-body failures fold into the envelope so
        // `resp.ok` is the single failure check (mirrors the Binance client).
        exchange::rest::RestResponse<Resp> err;
        err.errors.push_back(std::string("request failed: ") + e.what());
        return err;
    }
}

// Execute a private request — CoinbaseAuth signs with the CB-ACCESS-* headers.
template<typename Req, typename Enable>
exchange::rest::RestResponse<typename Req::response_type>
CoinbaseRestClient::execute(const Req& req, const CoinbaseCredentials& creds) {
    using Resp = typename Req::response_type;
    try {
        auto http = req.build();
        CoinbaseAuth(creds).sign(http);
        const HttpResponse r = perform_(http);
        return parse_coinbase_response<Resp>(r.status, json::parse(r.body));
    } catch (const std::exception& e) {
        exchange::rest::RestResponse<Resp> err;
        err.errors.push_back(std::string("request failed: ") + e.what());
        return err;
    }
}

} // namespace exchange::coinbase::rest
