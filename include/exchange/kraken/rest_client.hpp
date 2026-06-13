// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/kraken/rest_client.hpp
// Type-safe HTTP executor for the Kraken REST API. The libcurl transport is the
// shared exchange::rest::CurlHttpClient (plan 010); this class only binds the
// Kraken envelope/parse. Kraken reads errors from the JSON envelope, so it
// ignores the HTTP status the transport returns.
//
// Namespace: exchange::kraken::rest

#include "exchange/kraken/rest_api.hpp"
#include "exchange/common/http_client.hpp"

#include <functional>
#include <memory>
#include <type_traits>

namespace exchange::kraken::rest {

using exchange::rest::CurlHttpClient;
using exchange::rest::HttpResponse;

class KrakenRestClient {
public:
    // Production constructor — uses the shared libcurl transport.
    explicit KrakenRestClient(std::string base_url = "https://api.kraken.com");

    KrakenRestClient(const KrakenRestClient&) = delete;
    KrakenRestClient& operator=(const KrakenRestClient&) = delete;

    // Execute a public request (no credentials required).
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PublicRequest, Req>>>
    exchange::kraken::RestResponse<typename Req::response_type>
    execute(const Req& req) {
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
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PrivateRequest, Req>>>
    exchange::kraken::RestResponse<typename Req::response_type>
    execute(const Req& req, const Credentials& creds) {
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

private:
    // Test constructor — injects a body-only performer (Kraken keys errors off
    // the envelope, not the HTTP status, so tests only ever supply a body).
    explicit KrakenRestClient(std::function<std::string(const HttpRequest&)> performer);

    friend KrakenRestClient make_test_client(std::function<std::string(const HttpRequest&)>);

    std::shared_ptr<CurlHttpClient>                 transport_;  // null in tests
    std::function<HttpResponse(const HttpRequest&)> perform_;
};

// Factory used by unit tests to inject a mock HTTP performer (body only).
inline KrakenRestClient make_test_client(std::function<std::string(const HttpRequest&)> fn) {
    return KrakenRestClient(std::move(fn));
}

} // namespace exchange::kraken::rest
