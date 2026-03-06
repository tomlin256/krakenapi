// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_rest_client.hpp
// Type-safe HTTP executor for the Kraken REST API.
//
// Accepts TypedPublicRequest<R> or TypedPrivateRequest<R> instances,
// executes them via libcurl, parses the JSON response, and returns
// a typed RestResponse<R>.
//
// Usage (public):
//   KrakenRestClient client;
//   auto resp = client.execute(GetServerTimeRequest{});
//   if (resp.ok) std::cout << resp.result->unixtime;
//
// Usage (private):
//   KrakenRestClient client;
//   Credentials creds{api_key, api_secret};
//   auto resp = client.execute(GetAccountBalanceRequest{}, creds);

#include "kraken_rest_api.hpp"

#include <curl/curl.h>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace kraken::rest {

class KrakenRestClient {
public:
    // Production constructor — uses real libcurl.
    explicit KrakenRestClient(std::string base_url = "https://api.kraken.com");
    ~KrakenRestClient();

    KrakenRestClient(const KrakenRestClient&) = delete;
    KrakenRestClient& operator=(const KrakenRestClient&) = delete;

    // Execute a public request (no credentials required).
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PublicRequest, Req>>>
    kraken::RestResponse<typename Req::response_type>
    execute(const Req& req) {
        auto http = req.build();
        auto raw  = perform_(http);
        return kraken::parse_rest_response<typename Req::response_type>(json::parse(raw));
    }

    // Execute a private request (credentials required).
    template<typename Req,
             typename = std::enable_if_t<std::is_base_of_v<PrivateRequest, Req>>>
    kraken::RestResponse<typename Req::response_type>
    execute(const Req& req, const Credentials& creds) {
        auto http = req.build(creds);
        auto raw  = perform_(http);
        return kraken::parse_rest_response<typename Req::response_type>(json::parse(raw));
    }

private:
    // Test constructor — injects a custom performer so unit tests can run without curl.
    explicit KrakenRestClient(std::function<std::string(const HttpRequest&)> performer);

    friend KrakenRestClient make_test_client(std::function<std::string(const HttpRequest&)>);

    std::string curl_perform(const HttpRequest& http);
    static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata);

    std::string base_url_;
    CURL* curl_{nullptr};
    std::function<std::string(const HttpRequest&)> perform_;
};

// Factory used by unit tests to inject a mock HTTP performer.
inline KrakenRestClient make_test_client(std::function<std::string(const HttpRequest&)> fn) {
    return KrakenRestClient(std::move(fn));
}

} // namespace kraken::rest
