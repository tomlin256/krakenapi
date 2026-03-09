// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_rest_api.hpp
// Kraken Spot REST API – request builders and response parsers.
//
// Base URL: https://api.kraken.com
// Public:   GET  /0/public/<Method>
// Private:  POST /0/private/<Method>
//
// AUTHENTICATION (private endpoints only)
// ----------------------------------------
// 1. Generate a nonce: monotonically increasing uint64 (e.g. ms timestamp).
// 2. POST body is url-encoded or JSON.
// 3. Compute:
//      msg    = URI_path + SHA256(nonce_string + POST_body)
//      sign   = HMAC-SHA512(base64_decode(api_secret), msg)
//      header = base64_encode(sign)
// 4. Send headers:
//      API-Key:  <public api key>
//      API-Sign: <computed signature>
//
// This file provides the request/response types.
// Actual HTTP transport and signing are the caller's responsibility;
// PrivateRequestBase::sign() provides the signature calculation so that
// any HTTP library (libcurl, Boost.Beast, cpp-httplib …) can be used.
//
// Dependencies: kraken_types.hpp, nlohmann/json, OpenSSL (for signing helper)

#include "kraken_types.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdexcept>
#include <fstream>

// OpenSSL headers – only needed for sign().
// If you bring your own signing, remove these and the sign() body.
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace kraken::rest {

using json = nlohmann::json;

// ============================================================
// Signing utilities
// ============================================================

namespace detail {

inline std::string base64_decode(const std::string& in) {
    std::string out;
    int val = 0, bits = -8;
    static const std::string chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (unsigned char c : in) {
        if (c == '=') break;
        auto pos = chars.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + static_cast<int>(pos);
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

inline std::string base64_encode(const unsigned char* data, size_t len) {
    static const char* chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int val = (unsigned char)data[i] << 16;
        if (i + 1 < len) val |= (unsigned char)data[i+1] << 8;
        if (i + 2 < len) val |= (unsigned char)data[i+2];
        out.push_back(chars[(val >> 18) & 63]);
        out.push_back(chars[(val >> 12) & 63]);
        out.push_back(i + 1 < len ? chars[(val >> 6) & 63] : '=');
        out.push_back(i + 2 < len ? chars[val & 63] : '=');
    }
    return out;
}

// SHA-256 of a byte string, returning raw bytes.
inline std::string sha256(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);
    return std::string(reinterpret_cast<char*>(hash), SHA256_DIGEST_LENGTH);
}

// HMAC-SHA512 of `data` using `key`, returning raw bytes.
inline std::string hmac_sha512(const std::string& key, const std::string& data) {
    unsigned char result[64];
    unsigned int  len = 64;
    HMAC(EVP_sha512(),
         reinterpret_cast<const unsigned char*>(key.data()), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         result, &len);
    return std::string(reinterpret_cast<char*>(result), len);
}

// URL-encode a string (application/x-www-form-urlencoded).
inline std::string url_encode(const std::string& s) {
    std::ostringstream oss;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            oss << c;
        else
            oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return oss.str();
}

// Build url-encoded POST body from a flat key/value map.
inline std::string build_form_body(const std::map<std::string, std::string>& params) {
    std::string body;
    for (const auto& [k, v] : params) {
        if (!body.empty()) body += '&';
        body += url_encode(k) + '=' + url_encode(v);
    }
    return body;
}

} // namespace detail

// ============================================================
// Nonce helper
// ============================================================

// Returns a monotonically increasing nonce based on the system clock.
// Uses microseconds to match the KAPI nonce scale (16-digit numbers).
// Kraken rejects nonces smaller than the last one used for a given API key,
// so switching to a lower-resolution clock (e.g. ms) causes "invalid nonce"
// if the key was previously used with KAPI.
inline uint64_t make_nonce() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
}

// ============================================================
// Auth credential bundle
// ============================================================

class Credentials {
public:
    std::string api_key;     // public key  → API-Key header
    std::string api_secret;  // base64-encoded private key → used for signing

    static Credentials from_file(const std::string& name, const std::string& location="") {
        std::string dir = location.empty()
            ? std::string(getenv("HOME")) + "/.kraken"
            : location;

        std::string filepath = dir + "/" + name;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open key file: " + filepath);
        }

        Credentials cred;
        if (!std::getline(file, cred.api_key) || cred.api_key.empty()) {
            throw std::runtime_error("Missing or empty API key in: " + filepath);
        }
        if (!std::getline(file, cred.api_secret) || cred.api_secret.empty()) {
            throw std::runtime_error("Missing or empty private key in: " + filepath);
        }

        return cred;
    }

    // Compute the API-Sign header value.
    //   uri_path : e.g. "/0/private/AddOrder"
    //   nonce    : the nonce string included in post_body
    //   post_body: raw url-encoded POST body (must already include nonce=...)
    std::string sign(const std::string& uri_path,
                     const std::string& nonce,
                     const std::string& post_body) const {
        using namespace detail;
        std::string decoded_secret = base64_decode(api_secret);
        std::string sha256_input   = nonce + post_body;
        std::string hashed         = sha256(sha256_input);
        std::string message        = uri_path + hashed;
        std::string mac            = hmac_sha512(decoded_secret, message);
        return base64_encode(reinterpret_cast<const unsigned char*>(mac.data()), mac.size());
    }
};

// ============================================================
// Request base classes
//
// PublicRequest  – no auth, params go in query string (GET) or body (POST)
// PrivateRequest – adds nonce; sign() produces API-Sign header value
// ============================================================

// A prepared HTTP request ready for the transport layer.
class HttpRequest {
public:
    enum class Method { GET, POST };
    Method      method{Method::GET};
    std::string path;       // e.g. "/0/public/Ticker"
    std::string query;      // for GET requests, already url-encoded
    std::string body;       // for POST requests, url-encoded form body
    std::map<std::string, std::string> headers;
};

class PublicRequest {
public:
    virtual ~PublicRequest() = default;

    // Build an HttpRequest (GET with query string).
    virtual HttpRequest build() const = 0;
};

class PrivateRequest {
public:
    virtual ~PrivateRequest() = default;

    // Build a signed HttpRequest.
    virtual HttpRequest build(const Credentials& creds) const = 0;

protected:
    // Helper: given the uri_path and param map (without nonce),
    // insert nonce, build form body, sign, and return HttpRequest.
    HttpRequest make_private_request(const std::string& uri_path,
                                     std::map<std::string, std::string> params,
                                     const Credentials& creds) const {
        std::string nonce_str = std::to_string(make_nonce());
        params["nonce"] = nonce_str;
        std::string body = detail::build_form_body(params);
        std::string sign = creds.sign(uri_path, nonce_str, body);

        HttpRequest req;
        req.method          = HttpRequest::Method::POST;
        req.path            = uri_path;
        req.body            = body;
        req.headers["Content-Type"] = "application/x-www-form-urlencoded";
        req.headers["API-Key"]      = creds.api_key;
        req.headers["API-Sign"]     = sign;
        return req;
    }
};

// Typed bases — link each request type to its response type at compile time.
template<typename R>
class TypedPublicRequest : public PublicRequest {
public:
    using response_type = R;
};

template<typename R>
class TypedPrivateRequest : public PrivateRequest {
public:
    using response_type = R;
};

// Forward declarations of all response types so that TypedPublicRequest<R> /
// TypedPrivateRequest<R> base class instantiations compile before the response
// classes are fully defined further down in this file.
class ServerTime;
class SystemStatus;
class AssetInfoResult;
class AssetPairsResult;
class TickerResult;
class OHLCResult;
class OrderBookResult;
class RecentTradesResult;
class AccountBalanceResult;
class ExtendedBalanceResult;
class TradeBalance;
class OpenOrdersResult;
class ClosedOrdersResult;
class QueryOrdersResultWrapper;
class TradesHistoryResult;
class QueryTradesResultWrapper;
class OpenPositionsResult;
class LedgersResult;
class QueryLedgersResultWrapper;
class AddOrderResult;
class AddOrderBatchResult;
class EditOrderResult;
class AmendOrderResult;
class CancelOrderResult;
class CancelAllResult;
class CancelAllAfterResult;
class CancelOrderBatchResult;
class WebSocketsTokenResult;
class DepositMethodsResult;
class DepositAddressesResult;
class WithdrawResult;
class CancelWithdrawalResult;
class CreateSubaccountResult;
class EarnBoolResult;

// ============================================================
// ============================================================
//  MARKET DATA (public)
// ============================================================
// ============================================================

// --- GET /0/public/Time -------------------------------------------

class GetServerTimeRequest : public TypedPublicRequest<ServerTime> {
public:
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Time";
        return r;
    }
};

class ServerTime : public kraken::IRestResult {
private:
    int64_t     unixtime_{0};
    std::string rfc1123_;
public:
    int64_t            unixtime() const { return unixtime_; }
    const std::string& rfc1123()  const { return rfc1123_; }
    static ServerTime from_json(const json& j) {
        ServerTime t;
        t.unixtime_ = j.value("unixtime", int64_t{0});
        t.rfc1123_  = j.value("rfc1123", "");
        return t;
    }
};

// --- GET /0/public/SystemStatus -----------------------------------

class GetSystemStatusRequest : public TypedPublicRequest<SystemStatus> {
public:
    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/SystemStatus";
        return r;
    }
};

class SystemStatus : public kraken::IRestResult {
private:
    std::string status_;    // "online" | "cancel_only" | "post_only" | "limit_only" | "maintenance"
    std::string timestamp_; // RFC3339
public:
    const std::string& status()    const { return status_; }
    const std::string& timestamp() const { return timestamp_; }
    static SystemStatus from_json(const json& j) {
        SystemStatus s;
        s.status_    = j.value("status", "");
        s.timestamp_ = j.value("timestamp", "");
        return s;
    }
};

// --- GET /0/public/Assets -----------------------------------------

class GetAssetInfoRequest : public TypedPublicRequest<AssetInfoResult> {
public:
    std::optional<std::vector<std::string>> assets;  // omit = all
    std::optional<std::string>              aclass;  // default "currency"

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Assets";
        std::string q;
        if (assets && !assets->empty()) {
            q += "asset=";
            for (size_t i = 0; i < assets->size(); ++i) {
                if (i) q += ',';
                q += (*assets)[i];
            }
        }
        if (aclass) { if (!q.empty()) q += '&'; q += "aclass=" + *aclass; }
        r.query = q;
        return r;
    }
};

class AssetInfo {
private:
    std::string aclass_;
    std::string altname_;
    int         decimals_{0};
    int         display_decimals_{0};
public:
    const std::string& aclass()           const { return aclass_; }
    const std::string& altname()          const { return altname_; }
    int                decimals()         const { return decimals_; }
    int                display_decimals() const { return display_decimals_; }
    static AssetInfo from_json(const json& j) {
        AssetInfo a;
        a.aclass_           = j.value("aclass", "");
        a.altname_          = j.value("altname", "");
        a.decimals_         = j.value("decimals", 0);
        a.display_decimals_ = j.value("display_decimals", 0);
        return a;
    }
};

class AssetInfoResult : public kraken::IRestResult {
private:
    std::map<std::string, AssetInfo> assets_; // keyed by Kraken asset name
public:
    const std::map<std::string, AssetInfo>& assets() const { return assets_; }
    static AssetInfoResult from_json(const json& j) {
        AssetInfoResult r;
        for (const auto& [k, v] : j.items())
            r.assets_[k] = AssetInfo::from_json(v);
        return r;
    }
};

// --- GET /0/public/AssetPairs -------------------------------------

class GetAssetPairsRequest : public TypedPublicRequest<AssetPairsResult> {
public:
    std::optional<std::vector<std::string>> pairs;
    std::optional<std::string>              info;  // "info"|"leverage"|"fees"|"margin"

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/AssetPairs";
        std::string q;
        if (pairs && !pairs->empty()) {
            q += "pair=";
            for (size_t i = 0; i < pairs->size(); ++i) { if (i) q += ','; q += (*pairs)[i]; }
        }
        if (info) { if (!q.empty()) q += '&'; q += "info=" + *info; }
        r.query = q;
        return r;
    }
};

class AssetPairInfo {
private:
    std::string altname_;
    std::string wsname_;
    std::string base_;
    std::string quote_;
    int         pair_decimals_{0};
    int         lot_decimals_{0};
    double      ordermin_{0.0};
    double      costmin_{0.0};
    std::vector<std::vector<double>> fees_;
    std::vector<std::vector<double>> fees_maker_;
public:
    const std::string& altname()       const { return altname_; }
    const std::string& wsname()        const { return wsname_; }
    const std::string& base()          const { return base_; }
    const std::string& quote()         const { return quote_; }
    int                pair_decimals() const { return pair_decimals_; }
    int                lot_decimals()  const { return lot_decimals_; }
    double             ordermin()      const { return ordermin_; }
    double             costmin()       const { return costmin_; }
    const std::vector<std::vector<double>>& fees()       const { return fees_; }
    const std::vector<std::vector<double>>& fees_maker() const { return fees_maker_; }

    static AssetPairInfo from_json(const json& j) {
        AssetPairInfo p;
        p.altname_       = j.value("altname", "");
        p.wsname_        = j.value("wsname", "");
        p.base_          = j.value("base", "");
        p.quote_         = j.value("quote", "");
        p.pair_decimals_ = j.value("pair_decimals", 0);
        p.lot_decimals_  = j.value("lot_decimals", 0);
        if (j.contains("ordermin")) p.ordermin_ = std::stod(j["ordermin"].get<std::string>());
        if (j.contains("costmin"))  p.costmin_  = std::stod(j["costmin"].get<std::string>());
        return p;
    }
};

class AssetPairsResult : public kraken::IRestResult {
private:
    std::map<std::string, AssetPairInfo> pairs_;
public:
    const std::map<std::string, AssetPairInfo>& pairs() const { return pairs_; }
    static AssetPairsResult from_json(const json& j) {
        AssetPairsResult r;
        for (const auto& [k, v] : j.items())
            r.pairs_[k] = AssetPairInfo::from_json(v);
        return r;
    }
};

// --- GET /0/public/Ticker -----------------------------------------

class GetTickerRequest : public TypedPublicRequest<TickerResult> {
public:
    std::optional<std::vector<std::string>> pairs;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Ticker";
        if (pairs && !pairs->empty()) {
            r.query = "pair=";
            for (size_t i = 0; i < pairs->size(); ++i) { if (i) r.query += ','; r.query += (*pairs)[i]; }
        }
        return r;
    }
};

// Ticker fields are arrays: [value, 24h-value] or [price, wholeLot, lot]
class TickerInfo {
private:
    double  ask_{0.0};
    double  bid_{0.0};
    double  last_{0.0};
    double  volume_today_{0.0};
    double  volume_24h_{0.0};
    double  vwap_today_{0.0};
    double  vwap_24h_{0.0};
    int64_t trades_today_{0};
    int64_t trades_24h_{0};
    double  low_today_{0.0};
    double  low_24h_{0.0};
    double  high_today_{0.0};
    double  high_24h_{0.0};
    double  open_{0.0};
public:
    double  ask()          const { return ask_; }
    double  bid()          const { return bid_; }
    double  last()         const { return last_; }
    double  volume_today() const { return volume_today_; }
    double  volume_24h()   const { return volume_24h_; }
    double  vwap_today()   const { return vwap_today_; }
    double  vwap_24h()     const { return vwap_24h_; }
    int64_t trades_today() const { return trades_today_; }
    int64_t trades_24h()   const { return trades_24h_; }
    double  low_today()    const { return low_today_; }
    double  low_24h()      const { return low_24h_; }
    double  high_today()   const { return high_today_; }
    double  high_24h()     const { return high_24h_; }
    double  open()         const { return open_; }

    static TickerInfo from_json(const json& j) {
        TickerInfo t;
        // Each field is an array; index 0 = today, index 1 = 24h
        if (j.contains("a")) t.ask_           = std::stod(j["a"][0].get<std::string>());
        if (j.contains("b")) t.bid_           = std::stod(j["b"][0].get<std::string>());
        if (j.contains("c")) t.last_          = std::stod(j["c"][0].get<std::string>());
        if (j.contains("v")) { t.volume_today_ = std::stod(j["v"][0].get<std::string>()); t.volume_24h_ = std::stod(j["v"][1].get<std::string>()); }
        if (j.contains("p")) { t.vwap_today_   = std::stod(j["p"][0].get<std::string>()); t.vwap_24h_   = std::stod(j["p"][1].get<std::string>()); }
        if (j.contains("t")) { t.trades_today_ = j["t"][0].get<int64_t>();                t.trades_24h_ = j["t"][1].get<int64_t>(); }
        if (j.contains("l")) { t.low_today_    = std::stod(j["l"][0].get<std::string>()); t.low_24h_    = std::stod(j["l"][1].get<std::string>()); }
        if (j.contains("h")) { t.high_today_   = std::stod(j["h"][0].get<std::string>()); t.high_24h_   = std::stod(j["h"][1].get<std::string>()); }
        if (j.contains("o")) t.open_          = std::stod(j["o"].get<std::string>());
        return t;
    }
};

class TickerResult : public kraken::IRestResult {
private:
    std::map<std::string, TickerInfo> tickers_;
public:
    const std::map<std::string, TickerInfo>& tickers() const { return tickers_; }
    static TickerResult from_json(const json& j) {
        TickerResult r;
        for (const auto& [k, v] : j.items())
            r.tickers_[k] = TickerInfo::from_json(v);
        return r;
    }
};

// --- GET /0/public/OHLC -------------------------------------------

class GetOHLCRequest : public TypedPublicRequest<OHLCResult> {
public:
    std::string pair;
    std::optional<int32_t>  interval; // minutes: 1,5,15,30,60,240,1440,10080,21600
    std::optional<int64_t>  since;    // unix timestamp

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/OHLC";
        r.query  = "pair=" + pair;
        if (interval) r.query += "&interval=" + std::to_string(*interval);
        if (since)    r.query += "&since=" + std::to_string(*since);
        return r;
    }
};

class OHLCCandle {
private:
    int64_t time_{0};
    double  open_{0.0};
    double  high_{0.0};
    double  low_{0.0};
    double  close_{0.0};
    double  vwap_{0.0};
    double  volume_{0.0};
    int64_t count_{0};
public:
    int64_t time()   const { return time_; }
    double  open()   const { return open_; }
    double  high()   const { return high_; }
    double  low()    const { return low_; }
    double  close()  const { return close_; }
    double  vwap()   const { return vwap_; }
    double  volume() const { return volume_; }
    int64_t count()  const { return count_; }

    static OHLCCandle from_json(const json& c) {
        OHLCCandle candle;
        candle.time_   = c[0].get<int64_t>();
        candle.open_   = std::stod(c[1].get<std::string>());
        candle.high_   = std::stod(c[2].get<std::string>());
        candle.low_    = std::stod(c[3].get<std::string>());
        candle.close_  = std::stod(c[4].get<std::string>());
        candle.vwap_   = std::stod(c[5].get<std::string>());
        candle.volume_ = std::stod(c[6].get<std::string>());
        candle.count_  = c[7].get<int64_t>();
        return candle;
    }
};

class OHLCResult : public kraken::IRestResult {
private:
    std::string              pair_;
    std::vector<OHLCCandle>  candles_;
    int64_t                  last_{0};
public:
    const std::string&             pair()    const { return pair_; }
    const std::vector<OHLCCandle>& candles() const { return candles_; }
    int64_t                        last()    const { return last_; }

    static OHLCResult from_json(const json& j) {
        OHLCResult r;
        for (const auto& [k, v] : j.items()) {
            if (k == "last") { r.last_ = v.get<int64_t>(); continue; }
            r.pair_ = k;
            for (const auto& c : v)
                r.candles_.push_back(OHLCCandle::from_json(c));
        }
        return r;
    }
};

// --- GET /0/public/Depth ------------------------------------------

class GetOrderBookRequest : public TypedPublicRequest<OrderBookResult> {
public:
    std::string pair;
    std::optional<int32_t> count; // max 500

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Depth";
        r.query  = "pair=" + pair;
        if (count) r.query += "&count=" + std::to_string(*count);
        return r;
    }
};

class RestBookEntry {
private:
    double  price_{0.0};
    double  volume_{0.0};
    int64_t timestamp_{0};
public:
    double  price()     const { return price_; }
    double  volume()    const { return volume_; }
    int64_t timestamp() const { return timestamp_; }

    static RestBookEntry from_json(const json& e) {
        RestBookEntry r;
        r.price_     = std::stod(e[0].get<std::string>());
        r.volume_    = std::stod(e[1].get<std::string>());
        r.timestamp_ = e[2].get<int64_t>();
        return r;
    }
};

class OrderBookResult : public kraken::IRestResult {
private:
    std::string                 pair_;
    std::vector<RestBookEntry>  asks_;
    std::vector<RestBookEntry>  bids_;
public:
    const std::string&                pair() const { return pair_; }
    const std::vector<RestBookEntry>& asks() const { return asks_; }
    const std::vector<RestBookEntry>& bids() const { return bids_; }

    static OrderBookResult from_json(const json& j) {
        OrderBookResult r;
        for (const auto& [k, v] : j.items()) {
            r.pair_ = k;
            for (const auto& e : v["asks"]) r.asks_.push_back(RestBookEntry::from_json(e));
            for (const auto& e : v["bids"]) r.bids_.push_back(RestBookEntry::from_json(e));
        }
        return r;
    }
};

// --- GET /0/public/Trades -----------------------------------------

class GetRecentTradesRequest : public TypedPublicRequest<RecentTradesResult> {
public:
    std::string pair;
    std::optional<int64_t>  since;
    std::optional<int32_t>  count;

    HttpRequest build() const override {
        HttpRequest r;
        r.method = HttpRequest::Method::GET;
        r.path   = "/0/public/Trades";
        r.query  = "pair=" + pair;
        if (since) r.query += "&since=" + std::to_string(*since);
        if (count) r.query += "&count=" + std::to_string(*count);
        return r;
    }
};

class PublicTrade {
private:
    double      price_{0.0};
    double      volume_{0.0};
    double      time_{0.0};
    Side        side_{Side::Buy};
    std::string order_type_; // "l" limit, "m" market
    std::string misc_;
public:
    double             price()      const { return price_; }
    double             volume()     const { return volume_; }
    double             time()       const { return time_; }
    Side               side()       const { return side_; }
    const std::string& order_type() const { return order_type_; }
    const std::string& misc()       const { return misc_; }

    static PublicTrade from_json(const json& t) {
        PublicTrade pt;
        pt.price_      = std::stod(t[0].get<std::string>());
        pt.volume_     = std::stod(t[1].get<std::string>());
        pt.time_       = t[2].get<double>();
        pt.side_       = (t[3].get<std::string>() == "b") ? Side::Buy : Side::Sell;
        pt.order_type_ = t[4].get<std::string>();
        pt.misc_       = t[5].get<std::string>();
        return pt;
    }
};

class RecentTradesResult : public kraken::IRestResult {
private:
    std::string              pair_;
    std::vector<PublicTrade> trades_;
    std::string              last_;  // id for pagination
public:
    const std::string&              pair()   const { return pair_; }
    const std::vector<PublicTrade>& trades() const { return trades_; }
    const std::string&              last()   const { return last_; }

    static RecentTradesResult from_json(const json& j) {
        RecentTradesResult r;
        for (const auto& [k, v] : j.items()) {
            if (k == "last") { r.last_ = v.get<std::string>(); continue; }
            r.pair_ = k;
            for (const auto& t : v)
                r.trades_.push_back(PublicTrade::from_json(t));
        }
        return r;
    }
};

// ============================================================
// ============================================================
//  ACCOUNT DATA (private)
// ============================================================
// ============================================================

// --- POST /0/private/Balance --------------------------------------

class GetAccountBalanceRequest : public TypedPrivateRequest<AccountBalanceResult> {
public:
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/Balance", {}, creds);
    }
};

class AccountBalanceResult : public kraken::IRestResult {
private:
    std::map<std::string, double> balances_; // asset -> balance
public:
    const std::map<std::string, double>& balances() const { return balances_; }
    static AccountBalanceResult from_json(const json& j) {
        AccountBalanceResult r;
        for (const auto& [k, v] : j.items())
            r.balances_[k] = std::stod(v.get<std::string>());
        return r;
    }
};

// --- POST /0/private/BalanceEx ------------------------------------

class GetExtendedBalanceRequest : public TypedPrivateRequest<ExtendedBalanceResult> {
public:
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/BalanceEx", {}, creds);
    }
};

class ExtendedBalance {
private:
    double balance_{0.0};
    double hold_trade_{0.0};
    double credit_{0.0};
    double credit_used_{0.0};
public:
    double balance()     const { return balance_; }
    double hold_trade()  const { return hold_trade_; }
    double credit()      const { return credit_; }
    double credit_used() const { return credit_used_; }

    static ExtendedBalance from_json(const json& v) {
        ExtendedBalance b;
        b.balance_     = std::stod(v.value("balance", "0"));
        b.hold_trade_  = std::stod(v.value("hold_trade", "0"));
        b.credit_      = std::stod(v.value("credit", "0"));
        b.credit_used_ = std::stod(v.value("credit_used", "0"));
        return b;
    }
};

class ExtendedBalanceResult : public kraken::IRestResult {
private:
    std::map<std::string, ExtendedBalance> balances_;
public:
    const std::map<std::string, ExtendedBalance>& balances() const { return balances_; }
    static ExtendedBalanceResult from_json(const json& j) {
        ExtendedBalanceResult r;
        for (const auto& [k, v] : j.items())
            r.balances_[k] = ExtendedBalance::from_json(v);
        return r;
    }
};

// --- POST /0/private/TradeBalance ---------------------------------

class GetTradeBalanceRequest : public TypedPrivateRequest<TradeBalance> {
public:
    std::optional<std::string> asset; // base asset (default "ZUSD")

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (asset) p["asset"] = *asset;
        return make_private_request("/0/private/TradeBalance", p, creds);
    }
};

class TradeBalance : public kraken::IRestResult {
private:
    double eb_{0.0};  // equivalent balance
    double tb_{0.0};  // trade balance
    double m_{0.0};   // margin amount of open positions
    double n_{0.0};   // unrealized net P/L of open positions
    double c_{0.0};   // cost basis of open positions
    double v_{0.0};   // current floating valuation
    double e_{0.0};   // equity
    double mf_{0.0};  // free margin
    std::optional<double> ml_; // margin level
public:
    double eb() const { return eb_; }
    double tb() const { return tb_; }
    double m()  const { return m_; }
    double n()  const { return n_; }
    double c()  const { return c_; }
    double v()  const { return v_; }
    double e()  const { return e_; }
    double mf() const { return mf_; }
    std::optional<double> ml() const { return ml_; }

    static TradeBalance from_json(const json& j) {
        auto d = [&](const char* k) { return j.contains(k) ? std::stod(j[k].get<std::string>()) : 0.0; };
        TradeBalance t;
        t.eb_ = d("eb"); t.tb_ = d("tb"); t.m_ = d("m"); t.n_ = d("n");
        t.c_  = d("c");  t.v_  = d("v"); t.e_ = d("e"); t.mf_ = d("mf");
        if (j.contains("ml")) t.ml_ = std::stod(j["ml"].get<std::string>());
        return t;
    }
};

// --- POST /0/private/OpenOrders -----------------------------------

class GetOpenOrdersRequest : public TypedPrivateRequest<OpenOrdersResult> {
public:
    std::optional<bool>    trades;   // include trades in output
    std::optional<int64_t> userref;  // filter by userref

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (trades && *trades)  p["trades"]  = "true";
        if (userref)            p["userref"] = std::to_string(*userref);
        return make_private_request("/0/private/OpenOrders", p, creds);
    }
};

class OpenOrdersResult : public kraken::IRestResult {
private:
    std::map<std::string, kraken::OrderInfo> open_;
public:
    const std::map<std::string, kraken::OrderInfo>& open() const { return open_; }
    static OpenOrdersResult from_json(const json& j) {
        OpenOrdersResult r;
        if (j.contains("open"))
            for (const auto& [k, v] : j["open"].items())
                r.open_[k] = kraken::OrderInfo::from_json(v, k);
        return r;
    }
};

// --- POST /0/private/ClosedOrders ---------------------------------

class GetClosedOrdersRequest : public TypedPrivateRequest<ClosedOrdersResult> {
public:
    std::optional<bool>    trades;
    std::optional<int64_t> userref;
    std::optional<double>  start;
    std::optional<double>  end;
    std::optional<int32_t> ofs;       // result offset
    std::optional<std::string> closetime; // "open"|"close"|"both"

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (trades && *trades) p["trades"]    = "true";
        if (userref)           p["userref"]   = std::to_string(*userref);
        if (start)             p["start"]     = std::to_string(*start);
        if (end)               p["end"]       = std::to_string(*end);
        if (ofs)               p["ofs"]       = std::to_string(*ofs);
        if (closetime)         p["closetime"] = *closetime;
        return make_private_request("/0/private/ClosedOrders", p, creds);
    }
};

class ClosedOrdersResult : public kraken::IRestResult {
private:
    std::map<std::string, kraken::OrderInfo> closed_;
    int32_t count_{0};
public:
    const std::map<std::string, kraken::OrderInfo>& closed() const { return closed_; }
    int32_t count() const { return count_; }
    static ClosedOrdersResult from_json(const json& j) {
        ClosedOrdersResult r;
        if (j.contains("closed"))
            for (const auto& [k, v] : j["closed"].items())
                r.closed_[k] = kraken::OrderInfo::from_json(v, k);
        if (j.contains("count")) r.count_ = j["count"].get<int32_t>();
        return r;
    }
};

// --- POST /0/private/QueryOrders ----------------------------------

class QueryOrdersRequest : public TypedPrivateRequest<QueryOrdersResultWrapper> {
public:
    std::vector<std::string> txids;  // up to 50
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        std::string ids;
        for (size_t i = 0; i < txids.size(); ++i) { if (i) ids += ','; ids += txids[i]; }
        p["txid"] = ids;
        if (trades && *trades) p["trades"] = "true";
        return make_private_request("/0/private/QueryOrders", p, creds);
    }
};

using QueryOrdersResult = std::map<std::string, kraken::OrderInfo>;

class QueryOrdersResultWrapper : public kraken::IRestResult {
private:
    std::map<std::string, kraken::OrderInfo> orders_;
public:
    const std::map<std::string, kraken::OrderInfo>& orders() const { return orders_; }
    static QueryOrdersResultWrapper from_json(const json& j) {
        QueryOrdersResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.orders_[k] = kraken::OrderInfo::from_json(v, k);
        return r;
    }
};

// --- POST /0/private/TradesHistory --------------------------------

class GetTradesHistoryRequest : public TypedPrivateRequest<TradesHistoryResult> {
public:
    std::optional<std::string> type;    // "all"|"any position"|"closed position"|"closing position"|"no position"
    std::optional<bool>        trades;
    std::optional<double>      start;
    std::optional<double>      end;
    std::optional<int32_t>     ofs;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (type)              p["type"]   = *type;
        if (trades && *trades) p["trades"] = "true";
        if (start)             p["start"]  = std::to_string(*start);
        if (end)               p["end"]    = std::to_string(*end);
        if (ofs)               p["ofs"]    = std::to_string(*ofs);
        return make_private_request("/0/private/TradesHistory", p, creds);
    }
};

class TradesHistoryResult : public kraken::IRestResult {
private:
    std::map<std::string, kraken::TradeInfo> trades_;
    int32_t count_{0};
public:
    const std::map<std::string, kraken::TradeInfo>& trades() const { return trades_; }
    int32_t count() const { return count_; }
    static TradesHistoryResult from_json(const json& j) {
        TradesHistoryResult r;
        if (j.contains("trades"))
            for (const auto& [k, v] : j["trades"].items())
                r.trades_[k] = kraken::TradeInfo::from_json(v, k);
        if (j.contains("count")) r.count_ = j["count"].get<int32_t>();
        return r;
    }
};

// --- POST /0/private/QueryTrades ----------------------------------

class QueryTradesRequest : public TypedPrivateRequest<QueryTradesResultWrapper> {
public:
    std::vector<std::string> txids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        std::string ids;
        for (size_t i = 0; i < txids.size(); ++i) { if (i) ids += ','; ids += txids[i]; }
        p["txid"] = ids;
        if (trades && *trades) p["trades"] = "true";
        return make_private_request("/0/private/QueryTrades", p, creds);
    }
};

class QueryTradesResultWrapper : public kraken::IRestResult {
private:
    std::map<std::string, kraken::TradeInfo> trades_;
public:
    const std::map<std::string, kraken::TradeInfo>& trades() const { return trades_; }
    static QueryTradesResultWrapper from_json(const json& j) {
        QueryTradesResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.trades_[k] = kraken::TradeInfo::from_json(v, k);
        return r;
    }
};

// --- POST /0/private/OpenPositions --------------------------------

class GetOpenPositionsRequest : public TypedPrivateRequest<OpenPositionsResult> {
public:
    std::optional<std::vector<std::string>> txids;
    std::optional<bool> docalcs;
    std::optional<bool> consolidation;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (txids && !txids->empty()) {
            std::string ids;
            for (size_t i = 0; i < txids->size(); ++i) { if (i) ids += ','; ids += (*txids)[i]; }
            p["txid"] = ids;
        }
        if (docalcs && *docalcs)        p["docalcs"]       = "true";
        if (consolidation && *consolidation) p["consolidation"] = "market";
        return make_private_request("/0/private/OpenPositions", p, creds);
    }
};

class PositionInfo {
private:
    std::string ordertxid_;
    std::string pair_;
    double      time_{0.0};
    Side        type_{Side::Buy};
    OrderType   ordertype_{OrderType::Market};
    double      cost_{0.0};
    double      fee_{0.0};
    double      vol_{0.0};
    double      vol_closed_{0.0};
    double      margin_{0.0};
    double      value_{0.0};
    double      net_{0.0};
    std::string terms_;
    std::string rollovertm_;
    std::string misc_;
    std::string oflags_;
public:
    const std::string& ordertxid()  const { return ordertxid_; }
    const std::string& pair()       const { return pair_; }
    double             time()       const { return time_; }
    Side               type()       const { return type_; }
    OrderType          ordertype()  const { return ordertype_; }
    double             cost()       const { return cost_; }
    double             fee()        const { return fee_; }
    double             vol()        const { return vol_; }
    double             vol_closed() const { return vol_closed_; }
    double             margin()     const { return margin_; }
    double             value()      const { return value_; }
    double             net()        const { return net_; }
    const std::string& terms()      const { return terms_; }
    const std::string& rollovertm() const { return rollovertm_; }
    const std::string& misc()       const { return misc_; }
    const std::string& oflags()     const { return oflags_; }

    static PositionInfo from_json(const json& j) {
        PositionInfo p;
        p.ordertxid_  = j.value("ordertxid", "");
        p.pair_       = j.value("pair", "");
        p.time_       = j.value("time", 0.0);
        p.cost_       = std::stod(j.value("cost", "0"));
        p.fee_        = std::stod(j.value("fee", "0"));
        p.vol_        = std::stod(j.value("vol", "0"));
        p.vol_closed_ = std::stod(j.value("vol_closed", "0"));
        p.margin_     = std::stod(j.value("margin", "0"));
        p.terms_      = j.value("terms", "");
        p.rollovertm_ = j.value("rollovertm", "");
        p.misc_       = j.value("misc", "");
        p.oflags_     = j.value("oflags", "");
        if (j.contains("type"))      p.type_      = side_from_string(j["type"].get<std::string>());
        if (j.contains("ordertype")) p.ordertype_ = order_type_from_string(j["ordertype"].get<std::string>());
        if (j.contains("value"))     p.value_     = std::stod(j["value"].get<std::string>());
        if (j.contains("net"))       p.net_       = std::stod(j["net"].get<std::string>());
        return p;
    }
};

class OpenPositionsResult : public kraken::IRestResult {
private:
    std::map<std::string, PositionInfo> positions_;
public:
    const std::map<std::string, PositionInfo>& positions() const { return positions_; }
    static OpenPositionsResult from_json(const json& j) {
        OpenPositionsResult r;
        for (const auto& [k, v] : j.items())
            r.positions_[k] = PositionInfo::from_json(v);
        return r;
    }
};

// --- POST /0/private/Ledgers / QueryLedgers -----------------------

class GetLedgersRequest : public TypedPrivateRequest<LedgersResult> {
public:
    std::optional<std::vector<std::string>> assets;
    std::optional<std::string> aclass;
    std::optional<std::string> type;    // "all"|"trade"|"deposit"|"withdrawal"|...
    std::optional<double>  start;
    std::optional<double>  end;
    std::optional<int32_t> ofs;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (assets && !assets->empty()) {
            std::string a;
            for (size_t i = 0; i < assets->size(); ++i) { if (i) a += ','; a += (*assets)[i]; }
            p["asset"] = a;
        }
        if (aclass) p["aclass"] = *aclass;
        if (type)   p["type"]   = *type;
        if (start)  p["start"]  = std::to_string(*start);
        if (end)    p["end"]    = std::to_string(*end);
        if (ofs)    p["ofs"]    = std::to_string(*ofs);
        return make_private_request("/0/private/Ledgers", p, creds);
    }
};

class LedgersResult : public kraken::IRestResult {
private:
    std::map<std::string, kraken::LedgerEntry> ledger_;
    int32_t count_{0};
public:
    const std::map<std::string, kraken::LedgerEntry>& ledger() const { return ledger_; }
    int32_t count() const { return count_; }
    static LedgersResult from_json(const json& j) {
        LedgersResult r;
        if (j.contains("ledger"))
            for (const auto& [k, v] : j["ledger"].items())
                r.ledger_[k] = kraken::LedgerEntry::from_json(v, k);
        if (j.contains("count")) r.count_ = j["count"].get<int32_t>();
        return r;
    }
};

class QueryLedgersRequest : public TypedPrivateRequest<QueryLedgersResultWrapper> {
public:
    std::vector<std::string> ids;
    std::optional<bool>      trades;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        std::string id_str;
        for (size_t i = 0; i < ids.size(); ++i) { if (i) id_str += ','; id_str += ids[i]; }
        p["id"] = id_str;
        if (trades && *trades) p["trades"] = "true";
        return make_private_request("/0/private/QueryLedgers", p, creds);
    }
};

class QueryLedgersResultWrapper : public kraken::IRestResult {
private:
    std::map<std::string, kraken::LedgerEntry> ledger_;
public:
    const std::map<std::string, kraken::LedgerEntry>& ledger() const { return ledger_; }
    static QueryLedgersResultWrapper from_json(const json& j) {
        QueryLedgersResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.ledger_[k] = kraken::LedgerEntry::from_json(v, k);
        return r;
    }
};

// ============================================================
// ============================================================
//  TRADING (private)
// ============================================================
// ============================================================

// Helper: serialize an OrderParams into the REST param map.
// REST uses "pair" instead of "symbol", and field names differ slightly.
inline void apply_order_params_to_rest(std::map<std::string, std::string>& p,
                                       const kraken::OrderParams& op) {
    p["ordertype"] = kraken::to_string(op.order_type);
    p["type"]      = kraken::to_string(op.side);
    p["volume"]    = std::to_string(op.order_qty);
    p["pair"]      = op.symbol;

    if (op.limit_price)   p["price"]  = std::to_string(*op.limit_price);
    if (op.time_in_force) p["timeinforce"] = kraken::to_string(*op.time_in_force);
    if (op.margin && *op.margin) p["leverage"] = "5"; // caller may override
    if (op.post_only && *op.post_only)   p["oflags"] = "post";
    if (op.expire_time)   p["expiretm"] = *op.expire_time;
    if (op.cl_ord_id)     p["cl_ord_id"] = *op.cl_ord_id;
    if (op.order_userref) p["userref"]   = std::to_string(*op.order_userref);
    if (op.display_qty)   p["displayvol"] = std::to_string(*op.display_qty);
    if (op.validate && *op.validate) p["validate"] = "true";
    if (op.deadline)      p["deadline"]   = *op.deadline;

    // Triggers (stop price)
    if (op.triggers) p["price"] = std::to_string(op.triggers->price);

    // Conditional close (OTO)
    if (op.conditional) {
        if (op.conditional->order_type) p["close[ordertype]"] = kraken::to_string(*op.conditional->order_type);
        if (op.conditional->limit_price) p["close[price]"]    = std::to_string(*op.conditional->limit_price);
        if (op.conditional->trigger_price) p["close[price2]"] = std::to_string(*op.conditional->trigger_price);
    }
}

// --- POST /0/private/AddOrder -------------------------------------

class AddOrderRequest : public TypedPrivateRequest<AddOrderResult> {
public:
    kraken::OrderParams params;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        apply_order_params_to_rest(p, params);
        return make_private_request("/0/private/AddOrder", p, creds);
    }
};

class AddOrderResult : public kraken::IRestResult {
private:
    std::string              descr_order_;  // human-readable description
    std::optional<std::string> descr_close_;
    std::vector<std::string> txids_;
public:
    const std::string&              descr_order()  const { return descr_order_; }
    std::optional<std::string>      descr_close()  const { return descr_close_; }
    const std::vector<std::string>& txids()        const { return txids_; }

    static AddOrderResult from_json(const json& j) {
        AddOrderResult r;
        if (j.contains("descr")) {
            r.descr_order_ = j["descr"].value("order", "");
            if (j["descr"].contains("close")) r.descr_close_ = j["descr"]["close"].get<std::string>();
        }
        if (j.contains("txid"))
            r.txids_ = j["txid"].get<std::vector<std::string>>();
        return r;
    }
};

// --- POST /0/private/AddOrderBatch --------------------------------

class AddOrderBatchRequest : public TypedPrivateRequest<AddOrderBatchResult> {
public:
    std::string pair;
    std::vector<kraken::OrderParams> orders;
    std::optional<bool>   validate;
    std::optional<std::string> deadline;

    HttpRequest build(const Credentials& creds) const override {
        // Batch uses JSON body encoding
        json body = json::array();
        for (const auto& op : orders) {
            json o;
            o["ordertype"] = kraken::to_string(op.order_type);
            o["type"]      = kraken::to_string(op.side);
            o["volume"]    = std::to_string(op.order_qty);
            if (op.limit_price) o["price"] = std::to_string(*op.limit_price);
            if (op.cl_ord_id)   o["cl_ord_id"] = *op.cl_ord_id;
            if (op.order_userref) o["userref"] = *op.order_userref;
            body.push_back(o);
        }
        uint64_t n = make_nonce();
        std::string nonce_str = std::to_string(n);

        json req_body;
        req_body["nonce"]  = nonce_str;
        req_body["pair"]   = pair;
        req_body["orders"] = body;
        if (validate) req_body["validate"] = *validate;
        if (deadline) req_body["deadline"] = *deadline;

        std::string body_str = req_body.dump();
        std::string sign     = creds.sign("/0/private/AddOrderBatch", nonce_str, body_str);

        HttpRequest r;
        r.method = HttpRequest::Method::POST;
        r.path   = "/0/private/AddOrderBatch";
        r.body   = body_str;
        r.headers["Content-Type"] = "application/json";
        r.headers["API-Key"]      = creds.api_key;
        r.headers["API-Sign"]     = sign;
        return r;
    }
};

class BatchOrderResult {
private:
    std::string              descr_order_;
    std::vector<std::string> txids_;
    std::optional<std::string> error_;
public:
    const std::string&              descr_order() const { return descr_order_; }
    const std::vector<std::string>& txids()       const { return txids_; }
    std::optional<std::string>      error()       const { return error_; }

    static BatchOrderResult from_json(const json& o) {
        BatchOrderResult br;
        if (o.contains("descr")) br.descr_order_ = o["descr"].value("order", "");
        if (o.contains("txid"))  br.txids_        = o["txid"].get<std::vector<std::string>>();
        if (o.contains("error")) br.error_        = o["error"].get<std::string>();
        return br;
    }
};

class AddOrderBatchResult : public kraken::IRestResult {
private:
    std::vector<BatchOrderResult> orders_;
public:
    const std::vector<BatchOrderResult>& orders() const { return orders_; }
    static AddOrderBatchResult from_json(const json& j) {
        AddOrderBatchResult r;
        if (j.contains("orders")) {
            for (const auto& o : j["orders"])
                r.orders_.push_back(BatchOrderResult::from_json(o));
        }
        return r;
    }
};

// --- POST /0/private/EditOrder ------------------------------------

class EditOrderRequest : public TypedPrivateRequest<EditOrderResult> {
public:
    std::string txid;     // original order txid
    std::string pair;
    std::optional<double>      volume;
    std::optional<double>      price;
    std::optional<double>      price2;
    std::optional<double>      display_vol;
    std::optional<bool>        post_only;
    std::optional<std::string> deadline;
    std::optional<int64_t>     userref;
    std::optional<std::string> cl_ord_id;
    std::optional<bool>        validate;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        p["txid"] = txid;
        p["pair"] = pair;
        if (volume)      p["volume"]     = std::to_string(*volume);
        if (price)       p["price"]      = std::to_string(*price);
        if (price2)      p["price2"]     = std::to_string(*price2);
        if (display_vol) p["displayvol"] = std::to_string(*display_vol);
        if (post_only && *post_only) p["oflags"] = "post";
        if (deadline)    p["deadline"]   = *deadline;
        if (userref)     p["userref"]    = std::to_string(*userref);
        if (cl_ord_id)   p["cl_ord_id"]  = *cl_ord_id;
        if (validate && *validate) p["validate"] = "true";
        return make_private_request("/0/private/EditOrder", p, creds);
    }
};

class EditOrderResult : public kraken::IRestResult {
private:
    std::string              descr_order_;
    std::vector<std::string> txids_;
    std::optional<std::string> orig_txid_;
public:
    const std::string&              descr_order() const { return descr_order_; }
    const std::vector<std::string>& txids()       const { return txids_; }
    std::optional<std::string>      orig_txid()   const { return orig_txid_; }

    static EditOrderResult from_json(const json& j) {
        EditOrderResult r;
        if (j.contains("descr")) r.descr_order_ = j["descr"].value("order", "");
        if (j.contains("txid"))  r.txids_        = j["txid"].get<std::vector<std::string>>();
        if (j.contains("originaltxid")) r.orig_txid_ = j["originaltxid"].get<std::string>();
        return r;
    }
};

// --- POST /0/private/AmendOrder -----------------------------------

class AmendOrderRequest : public TypedPrivateRequest<AmendOrderResult> {
public:
    // Must provide one of:
    std::optional<std::string> txid;
    std::optional<std::string> cl_ord_id;

    std::optional<double>      order_qty;
    std::optional<double>      display_qty;
    std::optional<double>      limit_price;
    std::optional<double>      trigger_price;
    std::optional<std::string> deadline;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p;
        if (txid)          p["txid"]          = *txid;
        if (cl_ord_id)     p["cl_ord_id"]     = *cl_ord_id;
        if (order_qty)     p["order_qty"]     = std::to_string(*order_qty);
        if (display_qty)   p["display_qty"]   = std::to_string(*display_qty);
        if (limit_price)   p["limit_price"]   = std::to_string(*limit_price);
        if (trigger_price) p["trigger_price"] = std::to_string(*trigger_price);
        if (deadline)      p["deadline"]      = *deadline;
        return make_private_request("/0/private/AmendOrder", p, creds);
    }
};

class AmendOrderResult : public kraken::IRestResult {
private:
    std::string amend_id_;
public:
    const std::string& amend_id() const { return amend_id_; }
    static AmendOrderResult from_json(const json& j) {
        AmendOrderResult r;
        r.amend_id_ = j.value("amend_id", "");
        return r;
    }
};

// --- POST /0/private/CancelOrder ----------------------------------

class CancelOrderRequest : public TypedPrivateRequest<CancelOrderResult> {
public:
    std::string txid;  // txid or cl_ord_id

    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CancelOrder", {{"txid", txid}}, creds);
    }
};

class CancelOrderResult : public kraken::IRestResult {
private:
    int32_t count_{0};   // number of orders cancelled
    bool    pending_{false};
public:
    int32_t count()   const { return count_; }
    bool    pending() const { return pending_; }
    static CancelOrderResult from_json(const json& j) {
        CancelOrderResult r;
        r.count_   = j.value("count", 0);
        r.pending_ = j.value("pending", false);
        return r;
    }
};

// --- POST /0/private/CancelAll ------------------------------------

class CancelAllOrdersRequest : public TypedPrivateRequest<CancelAllResult> {
public:
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CancelAll", {}, creds);
    }
};

class CancelAllResult : public kraken::IRestResult {
private:
    int32_t count_{0};
public:
    int32_t count() const { return count_; }
    static CancelAllResult from_json(const json& j) {
        CancelAllResult r;
        r.count_ = j.value("count", 0);
        return r;
    }
};

// --- POST /0/private/CancelAllOrdersAfter -------------------------

class CancelAllOrdersAfterRequest : public TypedPrivateRequest<CancelAllAfterResult> {
public:
    int32_t timeout{0}; // seconds; 0 = disable

    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CancelAllOrdersAfter",
                                    {{"timeout", std::to_string(timeout)}}, creds);
    }
};

class CancelAllAfterResult : public kraken::IRestResult {
private:
    std::string current_time_;
    std::string trigger_time_;
public:
    const std::string& current_time() const { return current_time_; }
    const std::string& trigger_time() const { return trigger_time_; }
    static CancelAllAfterResult from_json(const json& j) {
        CancelAllAfterResult r;
        r.current_time_ = j.value("currentTime", "");
        r.trigger_time_ = j.value("triggerTime", "");
        return r;
    }
};

// --- POST /0/private/CancelOrderBatch -----------------------------

class CancelOrderBatchRequest : public TypedPrivateRequest<CancelOrderBatchResult> {
public:
    std::vector<std::string> orders;  // txids or cl_ord_ids

    HttpRequest build(const Credentials& creds) const override {
        // Uses JSON body
        uint64_t n = make_nonce();
        std::string nonce_str = std::to_string(n);
        json req;
        req["nonce"]  = nonce_str;
        req["orders"] = orders;
        std::string body_str = req.dump();
        std::string sign     = creds.sign("/0/private/CancelOrderBatch", nonce_str, body_str);

        HttpRequest r;
        r.method = HttpRequest::Method::POST;
        r.path   = "/0/private/CancelOrderBatch";
        r.body   = body_str;
        r.headers["Content-Type"] = "application/json";
        r.headers["API-Key"]      = creds.api_key;
        r.headers["API-Sign"]     = sign;
        return r;
    }
};

class CancelOrderBatchResult : public kraken::IRestResult {
private:
    int32_t count_{0};
public:
    int32_t count() const { return count_; }
    static CancelOrderBatchResult from_json(const json& j) {
        CancelOrderBatchResult r;
        r.count_ = j.value("count", 0);
        return r;
    }
};

// --- POST /0/private/GetWebSocketsToken ---------------------------

class GetWebSocketsTokenRequest : public TypedPrivateRequest<WebSocketsTokenResult> {
public:
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/GetWebSocketsToken", {}, creds);
    }
};

class WebSocketsTokenResult : public kraken::IRestResult {
private:
    std::string token_;
    int64_t     expires_{0};
public:
    const std::string& token()   const { return token_; }
    int64_t            expires() const { return expires_; }
    static WebSocketsTokenResult from_json(const json& j) {
        WebSocketsTokenResult r;
        r.token_   = j.value("token", "");
        r.expires_ = j.value("expires", int64_t{0});
        return r;
    }
};

// ============================================================
// FUNDING  (private)
// ============================================================

class GetDepositMethodsRequest : public TypedPrivateRequest<DepositMethodsResult> {
public:
    std::string asset;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/DepositMethods", {{"asset", asset}}, creds);
    }
};

class DepositMethod {
private:
    std::string method_;
    std::string limit_;
    std::string fee_;
    bool        gen_address_{false};
public:
    const std::string& method()      const { return method_; }
    const std::string& limit()       const { return limit_; }
    const std::string& fee()         const { return fee_; }
    bool               gen_address() const { return gen_address_; }
    static DepositMethod from_json(const json& j) {
        DepositMethod r;
        r.method_      = j.value("method", "");
        r.limit_       = j.value("limit", "");
        r.fee_         = j.value("fee", "");
        r.gen_address_ = j.value("gen-address", false);
        return r;
    }
};

class DepositMethodsResult : public kraken::IRestResult {
private:
    std::vector<DepositMethod> methods_;
public:
    const std::vector<DepositMethod>& methods() const { return methods_; }
    static DepositMethodsResult from_json(const json& j) {
        DepositMethodsResult r;
        for (const auto& m : j) r.methods_.push_back(DepositMethod::from_json(m));
        return r;
    }
};

class GetDepositAddressesRequest : public TypedPrivateRequest<DepositAddressesResult> {
public:
    std::string asset;
    std::string method;
    std::optional<bool> new_address;
    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p = {{"asset", asset}, {"method", method}};
        if (new_address && *new_address) p["new"] = "true";
        return make_private_request("/0/private/DepositAddresses", p, creds);
    }
};

class DepositAddress {
private:
    std::string address_;
    std::string expiretm_;
    bool        new_addr_{false};
public:
    const std::string& address()  const { return address_; }
    const std::string& expiretm() const { return expiretm_; }
    bool               new_addr() const { return new_addr_; }
    static DepositAddress from_json(const json& j) {
        DepositAddress r;
        r.address_  = j.value("address", "");
        r.expiretm_ = j.value("expiretm", "");
        r.new_addr_ = j.value("new", false);
        return r;
    }
};

class DepositAddressesResult : public kraken::IRestResult {
private:
    std::vector<DepositAddress> addresses_;
public:
    const std::vector<DepositAddress>& addresses() const { return addresses_; }
    static DepositAddressesResult from_json(const json& j) {
        DepositAddressesResult r;
        for (const auto& a : j) r.addresses_.push_back(DepositAddress::from_json(a));
        return r;
    }
};

class WithdrawRequest : public TypedPrivateRequest<WithdrawResult> {
public:
    std::string asset;
    std::string key;       // withdrawal key name from account settings
    std::string amount;    // string to preserve precision
    std::optional<std::string> address;
    std::optional<std::string> max_fee;

    HttpRequest build(const Credentials& creds) const override {
        std::map<std::string, std::string> p = {{"asset", asset}, {"key", key}, {"amount", amount}};
        if (address) p["address"] = *address;
        if (max_fee) p["max_fee"] = *max_fee;
        return make_private_request("/0/private/Withdraw", p, creds);
    }
};

class WithdrawResult : public kraken::IRestResult {
private:
    std::string refid_;
public:
    const std::string& refid() const { return refid_; }
    static WithdrawResult from_json(const json& j) {
        WithdrawResult r;
        r.refid_ = j.value("refid", "");
        return r;
    }
};

class CancelWithdrawalRequest : public TypedPrivateRequest<CancelWithdrawalResult> {
public:
    std::string asset;
    std::string refid;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/WithdrawCancel",
                                    {{"asset", asset}, {"refid", refid}}, creds);
    }
};

class CancelWithdrawalResult : public kraken::IRestResult {
private:
    bool result_{false};
public:
    bool result() const { return result_; }
    static CancelWithdrawalResult from_json(const json& j) {
        CancelWithdrawalResult r;
        r.result_ = j.get<bool>();
        return r;
    }
};

// ============================================================
// SUBACCOUNTS  (private)
// ============================================================

class CreateSubaccountRequest : public TypedPrivateRequest<CreateSubaccountResult> {
public:
    std::string username;
    std::string email;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/CreateSubaccount",
                                    {{"username", username}, {"email", email}}, creds);
    }
};

class CreateSubaccountResult : public kraken::IRestResult {
private:
    bool result_{false};
public:
    bool result() const { return result_; }
    static CreateSubaccountResult from_json(const json& j) {
        CreateSubaccountResult r;
        r.result_ = j.get<bool>();
        return r;
    }
};

// ============================================================
// EARN  (private)
// ============================================================

class AllocateEarnRequest : public TypedPrivateRequest<EarnBoolResult> {
public:
    std::string strategy_id;
    std::string amount;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/Earn/Allocate",
                                    {{"strategy_id", strategy_id}, {"amount", amount}}, creds);
    }
};

class DeallocateEarnRequest : public TypedPrivateRequest<EarnBoolResult> {
public:
    std::string strategy_id;
    std::string amount;
    HttpRequest build(const Credentials& creds) const override {
        return make_private_request("/0/private/Earn/Deallocate",
                                    {{"strategy_id", strategy_id}, {"amount", amount}}, creds);
    }
};

class EarnBoolResult : public kraken::IRestResult {
private:
    bool result_{false};
public:
    bool result() const { return result_; }
    static EarnBoolResult from_json(const json& j) {
        EarnBoolResult r;
        r.result_ = j.get<bool>();
        return r;
    }
};

} // namespace kraken::rest
