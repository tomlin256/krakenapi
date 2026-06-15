// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/rest_client.hpp
// Type-safe HTTP executor for the Binance Spot REST API. The libcurl transport
// is the shared exchange::rest::CurlHttpClient (plan 010); this class only binds
// the Binance envelope/parse and auth.
//
// Namespace: exchange::binance::rest

#include "exchange/binance/rest_api.hpp"
#include "exchange/binance/auth.hpp"
#include "exchange/common/http_client.hpp"

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace exchange::binance::rest {

using exchange::rest::CurlHttpClient;
using exchange::rest::HttpResponse;

class BinanceRestClient {
public:
    // Production constructor — uses the shared libcurl transport.
    explicit BinanceRestClient(std::string base_url = "https://api.binance.com");

    BinanceRestClient(const BinanceRestClient&) = delete;
    BinanceRestClient& operator=(const BinanceRestClient&) = delete;

    // Execute a public request (no credentials required).
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PublicRequest, Req>>>
    exchange::rest::RestResponse<typename Req::response_type>
    execute(const Req& req) {
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
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PrivateRequest, Req>>>
    exchange::rest::RestResponse<typename Req::response_type>
    execute(const Req& req, const BinanceAuth& auth) {
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

private:
    // Test constructor — injects a custom performer so unit tests run without curl.
    explicit BinanceRestClient(
        std::function<HttpResponse(const HttpRequest&)> performer);

    friend BinanceRestClient make_binance_test_client(
        std::function<HttpResponse(const HttpRequest&)>);

    std::shared_ptr<CurlHttpClient>                 transport_;  // null in tests
    std::function<HttpResponse(const HttpRequest&)> perform_;
};

// Factory used by unit tests to inject a mock HTTP performer.
inline BinanceRestClient
make_binance_test_client(std::function<HttpResponse(const HttpRequest&)> fn) {
    return BinanceRestClient(std::move(fn));
}

} // namespace exchange::binance::rest
