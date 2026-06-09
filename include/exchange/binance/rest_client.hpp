// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/rest_client.hpp
// Type-safe HTTP executor for the Binance Spot REST API.
//
// Namespace: exchange::binance::rest

#include "exchange/binance/rest_api.hpp"
#include "exchange/binance/auth.hpp"

#include <curl/curl.h>
#include <functional>
#include <type_traits>
#include <utility>

namespace exchange::binance::rest {

class BinanceRestClient {
public:
    // Production constructor — uses real libcurl.
    explicit BinanceRestClient(std::string base_url = "https://api.binance.com");
    ~BinanceRestClient();

    BinanceRestClient(const BinanceRestClient&) = delete;
    BinanceRestClient& operator=(const BinanceRestClient&) = delete;

    // Execute a public request (no credentials required).
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PublicRequest, Req>>>
    exchange::rest::RestResponse<typename Req::response_type>
    execute(const Req& req) {
        auto http = req.build();
        auto [status, raw] = perform_(http);
        return parse_binance_response<typename Req::response_type>(
            status, json::parse(raw));
    }

    // Execute a private request (BinanceAuth injects timestamp + signature).
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PrivateRequest, Req>>>
    exchange::rest::RestResponse<typename Req::response_type>
    execute(const Req& req, const BinanceAuth& auth) {
        auto http = req.build();
        auth.sign(http);
        auto [status, raw] = perform_(http);
        return parse_binance_response<typename Req::response_type>(
            status, json::parse(raw));
    }

private:
    // Test constructor — injects a custom performer so unit tests run without curl.
    explicit BinanceRestClient(
        std::function<std::pair<int, std::string>(const HttpRequest&)> performer);

    friend BinanceRestClient make_binance_test_client(
        std::function<std::pair<int, std::string>(const HttpRequest&)>);

    std::pair<int, std::string> curl_perform(const HttpRequest& http);
    static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata);

    std::string   base_url_;
    CURL*         curl_{nullptr};
    std::function<std::pair<int, std::string>(const HttpRequest&)> perform_;
};

// Factory used by unit tests to inject a mock HTTP performer.
inline BinanceRestClient
make_binance_test_client(
    std::function<std::pair<int, std::string>(const HttpRequest&)> fn) {
    return BinanceRestClient(std::move(fn));
}

} // namespace exchange::binance::rest
