#include "kraken_rest_client.hpp"

#include <stdexcept>
#include <string>

namespace kraken::rest {

// ---------------------------------------------------------------------------
// write_cb — curl write callback; appends received data to a std::string.
// ---------------------------------------------------------------------------
size_t KrakenRestClient::write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = static_cast<std::string*>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// ---------------------------------------------------------------------------
// Production constructor — initialises libcurl and wires perform_ to curl_perform.
// ---------------------------------------------------------------------------
KrakenRestClient::KrakenRestClient(std::string base_url)
    : base_url_(std::move(base_url))
    , curl_(curl_easy_init())
{
    if (!curl_)
        throw std::runtime_error("KrakenRestClient: curl_easy_init() failed");
    perform_ = [this](const HttpRequest& h) { return curl_perform(h); };
}

// ---------------------------------------------------------------------------
// Test constructor — uses an injected performer; curl_ stays null.
// ---------------------------------------------------------------------------
KrakenRestClient::KrakenRestClient(std::function<std::string(const HttpRequest&)> performer)
    : base_url_{}
    , curl_(nullptr)
    , perform_(std::move(performer))
{}

KrakenRestClient::~KrakenRestClient() {
    if (curl_)
        curl_easy_cleanup(curl_);
}

// ---------------------------------------------------------------------------
// curl_perform — executes an HttpRequest with libcurl and returns the response.
// ---------------------------------------------------------------------------
std::string KrakenRestClient::curl_perform(const HttpRequest& http) {
    // Build full URL.
    std::string url = base_url_ + http.path;
    if (!http.query.empty())
        url += '?' + http.query;

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

    // Build header list from HttpRequest::headers.
    curl_slist* chunk = nullptr;
    for (const auto& [key, val] : http.headers)
        chunk = curl_slist_append(chunk, (key + ": " + val).c_str());
    if (chunk)
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, chunk);

    // Method-specific options.
    if (http.method == HttpRequest::Method::GET) {
        curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
    } else {
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, http.body.c_str());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, static_cast<long>(http.body.size()));
    }

    // Response buffer.
    std::string response;
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, KrakenRestClient::write_cb);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, static_cast<void*>(&response));

    CURLcode rc = curl_easy_perform(curl_);
    if (chunk)
        curl_slist_free_all(chunk);

    if (rc != CURLE_OK)
        throw std::runtime_error(std::string("curl_easy_perform failed: ") + curl_easy_strerror(rc));

    return response;
}

} // namespace kraken::rest
