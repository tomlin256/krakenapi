// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/binance/rest_client.hpp"

#include <stdexcept>
#include <string>

namespace exchange::binance::rest {

// ---------------------------------------------------------------------------
// write_cb — curl write callback; appends received data to a std::string.
// ---------------------------------------------------------------------------
size_t BinanceRestClient::write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// ---------------------------------------------------------------------------
// Production constructor — initialises libcurl and wires perform_ to curl_perform.
// ---------------------------------------------------------------------------
BinanceRestClient::BinanceRestClient(std::string base_url)
    : base_url_(std::move(base_url))
    , curl_(curl_easy_init())
{
    if (!curl_)
        throw std::runtime_error("BinanceRestClient: curl_easy_init() failed");
    perform_ = [this](const HttpRequest& h) { return curl_perform(h); };
}

// ---------------------------------------------------------------------------
// Test constructor — uses an injected performer; curl_ stays null.
// ---------------------------------------------------------------------------
BinanceRestClient::BinanceRestClient(
    std::function<std::pair<int, std::string>(const HttpRequest&)> performer)
    : base_url_{}
    , curl_(nullptr)
    , perform_(std::move(performer))
{}

BinanceRestClient::~BinanceRestClient() {
    if (curl_)
        curl_easy_cleanup(curl_);
}

// ---------------------------------------------------------------------------
// curl_perform — executes an HttpRequest with libcurl and returns
//                {http_status_code, response_body}.
// ---------------------------------------------------------------------------
std::pair<int, std::string> BinanceRestClient::curl_perform(const HttpRequest& http) {
    // Build full URL.
    std::string url = base_url_ + http.path;
    if (!http.query.empty())
        url += '?' + http.query;

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

    // Build header list from HttpRequest::headers.
    auto chunk_deleter = [](curl_slist* p) { if (p) curl_slist_free_all(p); };
    std::unique_ptr<curl_slist, decltype(chunk_deleter)> chunk(nullptr, chunk_deleter);
    for (const auto& [key, val] : http.headers)
        chunk.reset(curl_slist_append(chunk.release(), (key + ": " + val).c_str()));
    if (chunk)
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, chunk.get());

    // Method-specific options.
    if (http.method == HttpRequest::Method::GET) {
        curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    } else if (http.method == HttpRequest::Method::DELETE) {
        curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
        if (!http.body.empty()) {
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, http.body.c_str());
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                             static_cast<long>(http.body.size()));
        }
    } else {
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, http.body.c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(http.body.size()));
    }

    // Response buffer.
    std::string response;
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, BinanceRestClient::write_cb);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, static_cast<void*>(&response));

    CURLcode rc = curl_easy_perform(curl_);
    if (rc != CURLE_OK)
        throw std::runtime_error(
            std::string("curl_easy_perform failed: ") + curl_easy_strerror(rc));

    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);

    return {static_cast<int>(http_code), response};
}

} // namespace exchange::binance::rest
