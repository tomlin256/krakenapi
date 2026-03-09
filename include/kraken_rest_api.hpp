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
public:
    int64_t     unixtime{0};
    std::string rfc1123;
    static ServerTime from_json(const json& j) {
        ServerTime t;
        t.unixtime = j.value("unixtime", int64_t{0});
        t.rfc1123  = j.value("rfc1123", "");
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
public:
    std::string status;    // "online" | "cancel_only" | "post_only" | "limit_only" | "maintenance"
    std::string timestamp; // RFC3339
    static SystemStatus from_json(const json& j) {
        SystemStatus s;
        s.status    = j.value("status", "");
        s.timestamp = j.value("timestamp", "");
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
public:
    std::string aclass;
    std::string altname;
    int         decimals{0};
    int         display_decimals{0};
    static AssetInfo from_json(const json& j) {
        AssetInfo a;
        a.aclass           = j.value("aclass", "");
        a.altname          = j.value("altname", "");
        a.decimals         = j.value("decimals", 0);
        a.display_decimals = j.value("display_decimals", 0);
        return a;
    }
};

class AssetInfoResult : public kraken::IRestResult {
public:
    std::map<std::string, AssetInfo> assets; // keyed by Kraken asset name
    static AssetInfoResult from_json(const json& j) {
        AssetInfoResult r;
        for (const auto& [k, v] : j.items())
            r.assets[k] = AssetInfo::from_json(v);
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
public:
    std::string altname;
    std::string wsname;
    std::string base;
    std::string quote;
    int         pair_decimals{0};
    int         lot_decimals{0};
    double      ordermin{0.0};
    double      costmin{0.0};
    std::vector<std::vector<double>> fees;
    std::vector<std::vector<double>> fees_maker;

    static AssetPairInfo from_json(const json& j) {
        AssetPairInfo p;
        p.altname       = j.value("altname", "");
        p.wsname        = j.value("wsname", "");
        p.base          = j.value("base", "");
        p.quote         = j.value("quote", "");
        p.pair_decimals = j.value("pair_decimals", 0);
        p.lot_decimals  = j.value("lot_decimals", 0);
        if (j.contains("ordermin")) p.ordermin = std::stod(j["ordermin"].get<std::string>());
        if (j.contains("costmin"))  p.costmin  = std::stod(j["costmin"].get<std::string>());
        return p;
    }
};

class AssetPairsResult : public kraken::IRestResult {
public:
    std::map<std::string, AssetPairInfo> pairs;
    static AssetPairsResult from_json(const json& j) {
        AssetPairsResult r;
        for (const auto& [k, v] : j.items())
            r.pairs[k] = AssetPairInfo::from_json(v);
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
public:
    double ask{0.0};
    double bid{0.0};
    double last{0.0};
    double volume_today{0.0};
    double volume_24h{0.0};
    double vwap_today{0.0};
    double vwap_24h{0.0};
    int64_t trades_today{0};
    int64_t trades_24h{0};
    double  low_today{0.0};
    double  low_24h{0.0};
    double  high_today{0.0};
    double  high_24h{0.0};
    double  open{0.0};

    static TickerInfo from_json(const json& j) {
        TickerInfo t;
        // Each field is an array; index 0 = today, index 1 = 24h
        if (j.contains("a")) t.ask           = std::stod(j["a"][0].get<std::string>());
        if (j.contains("b")) t.bid           = std::stod(j["b"][0].get<std::string>());
        if (j.contains("c")) t.last          = std::stod(j["c"][0].get<std::string>());
        if (j.contains("v")) { t.volume_today = std::stod(j["v"][0].get<std::string>()); t.volume_24h = std::stod(j["v"][1].get<std::string>()); }
        if (j.contains("p")) { t.vwap_today   = std::stod(j["p"][0].get<std::string>()); t.vwap_24h   = std::stod(j["p"][1].get<std::string>()); }
        if (j.contains("t")) { t.trades_today = j["t"][0].get<int64_t>();                t.trades_24h = j["t"][1].get<int64_t>(); }
        if (j.contains("l")) { t.low_today    = std::stod(j["l"][0].get<std::string>()); t.low_24h    = std::stod(j["l"][1].get<std::string>()); }
        if (j.contains("h")) { t.high_today   = std::stod(j["h"][0].get<std::string>()); t.high_24h   = std::stod(j["h"][1].get<std::string>()); }
        if (j.contains("o")) t.open          = std::stod(j["o"].get<std::string>());
        return t;
    }
};

class TickerResult : public kraken::IRestResult {
public:
    std::map<std::string, TickerInfo> tickers;
    static TickerResult from_json(const json& j) {
        TickerResult r;
        for (const auto& [k, v] : j.items())
            r.tickers[k] = TickerInfo::from_json(v);
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
public:
    int64_t time{0};
    double  open{0.0};
    double  high{0.0};
    double  low{0.0};
    double  close{0.0};
    double  vwap{0.0};
    double  volume{0.0};
    int64_t count{0};
};

class OHLCResult : public kraken::IRestResult {
public:
    std::string              pair;
    std::vector<OHLCCandle>  candles;
    int64_t                  last{0};

    static OHLCResult from_json(const json& j) {
        OHLCResult r;
        for (const auto& [k, v] : j.items()) {
            if (k == "last") { r.last = v.get<int64_t>(); continue; }
            r.pair = k;
            for (const auto& c : v) {
                OHLCCandle candle;
                candle.time   = c[0].get<int64_t>();
                candle.open   = std::stod(c[1].get<std::string>());
                candle.high   = std::stod(c[2].get<std::string>());
                candle.low    = std::stod(c[3].get<std::string>());
                candle.close  = std::stod(c[4].get<std::string>());
                candle.vwap   = std::stod(c[5].get<std::string>());
                candle.volume = std::stod(c[6].get<std::string>());
                candle.count  = c[7].get<int64_t>();
                r.candles.push_back(candle);
            }
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

class RestBookEntry { public: double price{0.0}; double volume{0.0}; int64_t timestamp{0}; };

class OrderBookResult : public kraken::IRestResult {
public:
    std::string                 pair;
    std::vector<RestBookEntry>  asks;
    std::vector<RestBookEntry>  bids;

    static OrderBookResult from_json(const json& j) {
        OrderBookResult r;
        for (const auto& [k, v] : j.items()) {
            r.pair = k;
            auto parse = [](const json& arr) {
                std::vector<RestBookEntry> entries;
                for (const auto& e : arr)
                    entries.push_back({std::stod(e[0].get<std::string>()),
                                       std::stod(e[1].get<std::string>()),
                                       e[2].get<int64_t>()});
                return entries;
            };
            r.asks = parse(v["asks"]);
            r.bids = parse(v["bids"]);
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
public:
    double      price{0.0};
    double      volume{0.0};
    double      time{0.0};
    Side        side{Side::Buy};
    std::string order_type; // "l" limit, "m" market
    std::string misc;
};

class RecentTradesResult : public kraken::IRestResult {
public:
    std::string              pair;
    std::vector<PublicTrade> trades;
    std::string              last;  // id for pagination

    static RecentTradesResult from_json(const json& j) {
        RecentTradesResult r;
        for (const auto& [k, v] : j.items()) {
            if (k == "last") { r.last = v.get<std::string>(); continue; }
            r.pair = k;
            for (const auto& t : v) {
                PublicTrade pt;
                pt.price      = std::stod(t[0].get<std::string>());
                pt.volume     = std::stod(t[1].get<std::string>());
                pt.time       = t[2].get<double>();
                pt.side       = (t[3].get<std::string>() == "b") ? Side::Buy : Side::Sell;
                pt.order_type = t[4].get<std::string>();
                pt.misc       = t[5].get<std::string>();
                r.trades.push_back(pt);
            }
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
public:
    std::map<std::string, double> balances; // asset -> balance
    static AccountBalanceResult from_json(const json& j) {
        AccountBalanceResult r;
        for (const auto& [k, v] : j.items())
            r.balances[k] = std::stod(v.get<std::string>());
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
public:
    double balance{0.0};
    double hold_trade{0.0};
    double credit{0.0};
    double credit_used{0.0};
};

class ExtendedBalanceResult : public kraken::IRestResult {
public:
    std::map<std::string, ExtendedBalance> balances;
    static ExtendedBalanceResult from_json(const json& j) {
        ExtendedBalanceResult r;
        for (const auto& [k, v] : j.items()) {
            ExtendedBalance b;
            b.balance     = std::stod(v.value("balance", "0"));
            b.hold_trade  = std::stod(v.value("hold_trade", "0"));
            b.credit      = std::stod(v.value("credit", "0"));
            b.credit_used = std::stod(v.value("credit_used", "0"));
            r.balances[k] = b;
        }
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
public:
    double eb{0.0};  // equivalent balance
    double tb{0.0};  // trade balance
    double m{0.0};   // margin amount of open positions
    double n{0.0};   // unrealized net P/L of open positions
    double c{0.0};   // cost basis of open positions
    double v{0.0};   // current floating valuation
    double e{0.0};   // equity
    double mf{0.0};  // free margin
    std::optional<double> ml; // margin level

    static TradeBalance from_json(const json& j) {
        auto d = [&](const char* k) { return j.contains(k) ? std::stod(j[k].get<std::string>()) : 0.0; };
        TradeBalance t;
        t.eb = d("eb"); t.tb = d("tb"); t.m = d("m"); t.n = d("n");
        t.c  = d("c");  t.v  = d("v"); t.e = d("e"); t.mf = d("mf");
        if (j.contains("ml")) t.ml = std::stod(j["ml"].get<std::string>());
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
public:
    std::map<std::string, kraken::OrderInfo> open;
    static OpenOrdersResult from_json(const json& j) {
        OpenOrdersResult r;
        if (j.contains("open"))
            for (const auto& [k, v] : j["open"].items())
                r.open[k] = kraken::OrderInfo::from_json(v, k);
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
public:
    std::map<std::string, kraken::OrderInfo> closed;
    int32_t count{0};
    static ClosedOrdersResult from_json(const json& j) {
        ClosedOrdersResult r;
        if (j.contains("closed"))
            for (const auto& [k, v] : j["closed"].items())
                r.closed[k] = kraken::OrderInfo::from_json(v, k);
        if (j.contains("count")) r.count = j["count"].get<int32_t>();
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
public:
    std::map<std::string, kraken::OrderInfo> orders;
    static QueryOrdersResultWrapper from_json(const json& j) {
        QueryOrdersResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.orders[k] = kraken::OrderInfo::from_json(v, k);
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
public:
    std::map<std::string, kraken::TradeInfo> trades;
    int32_t count{0};
    static TradesHistoryResult from_json(const json& j) {
        TradesHistoryResult r;
        if (j.contains("trades"))
            for (const auto& [k, v] : j["trades"].items())
                r.trades[k] = kraken::TradeInfo::from_json(v, k);
        if (j.contains("count")) r.count = j["count"].get<int32_t>();
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
public:
    std::map<std::string, kraken::TradeInfo> trades;
    static QueryTradesResultWrapper from_json(const json& j) {
        QueryTradesResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.trades[k] = kraken::TradeInfo::from_json(v, k);
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
public:
    std::string ordertxid;
    std::string pair;
    double      time{0.0};
    Side        type{Side::Buy};
    OrderType   ordertype{OrderType::Market};
    double      cost{0.0};
    double      fee{0.0};
    double      vol{0.0};
    double      vol_closed{0.0};
    double      margin{0.0};
    double      value{0.0};
    double      net{0.0};
    std::string terms;
    std::string rollovertm;
    std::string misc;
    std::string oflags;

    static PositionInfo from_json(const json& j) {
        PositionInfo p;
        p.ordertxid  = j.value("ordertxid", "");
        p.pair       = j.value("pair", "");
        p.time       = j.value("time", 0.0);
        p.cost       = std::stod(j.value("cost", "0"));
        p.fee        = std::stod(j.value("fee", "0"));
        p.vol        = std::stod(j.value("vol", "0"));
        p.vol_closed = std::stod(j.value("vol_closed", "0"));
        p.margin     = std::stod(j.value("margin", "0"));
        p.terms      = j.value("terms", "");
        p.rollovertm = j.value("rollovertm", "");
        p.misc       = j.value("misc", "");
        p.oflags     = j.value("oflags", "");
        if (j.contains("type"))      p.type      = side_from_string(j["type"].get<std::string>());
        if (j.contains("ordertype")) p.ordertype = order_type_from_string(j["ordertype"].get<std::string>());
        if (j.contains("value"))     p.value     = std::stod(j["value"].get<std::string>());
        if (j.contains("net"))       p.net       = std::stod(j["net"].get<std::string>());
        return p;
    }
};

class OpenPositionsResult : public kraken::IRestResult {
public:
    std::map<std::string, PositionInfo> positions;
    static OpenPositionsResult from_json(const json& j) {
        OpenPositionsResult r;
        for (const auto& [k, v] : j.items())
            r.positions[k] = PositionInfo::from_json(v);
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
public:
    std::map<std::string, kraken::LedgerEntry> ledger;
    int32_t count{0};
    static LedgersResult from_json(const json& j) {
        LedgersResult r;
        if (j.contains("ledger"))
            for (const auto& [k, v] : j["ledger"].items())
                r.ledger[k] = kraken::LedgerEntry::from_json(v, k);
        if (j.contains("count")) r.count = j["count"].get<int32_t>();
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
public:
    std::map<std::string, kraken::LedgerEntry> ledger;
    static QueryLedgersResultWrapper from_json(const json& j) {
        QueryLedgersResultWrapper r;
        for (const auto& [k, v] : j.items())
            r.ledger[k] = kraken::LedgerEntry::from_json(v, k);
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
public:
    std::string              descr_order;  // human-readable description
    std::optional<std::string> descr_close;
    std::vector<std::string> txids;

    static AddOrderResult from_json(const json& j) {
        AddOrderResult r;
        if (j.contains("descr")) {
            r.descr_order = j["descr"].value("order", "");
            if (j["descr"].contains("close")) r.descr_close = j["descr"]["close"].get<std::string>();
        }
        if (j.contains("txid"))
            r.txids = j["txid"].get<std::vector<std::string>>();
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
public:
    std::string              descr_order;
    std::vector<std::string> txids;
    std::optional<std::string> error;
};

class AddOrderBatchResult : public kraken::IRestResult {
public:
    std::vector<BatchOrderResult> orders;
    static AddOrderBatchResult from_json(const json& j) {
        AddOrderBatchResult r;
        if (j.contains("orders")) {
            for (const auto& o : j["orders"]) {
                BatchOrderResult br;
                if (o.contains("descr")) br.descr_order = o["descr"].value("order", "");
                if (o.contains("txid"))  br.txids        = o["txid"].get<std::vector<std::string>>();
                if (o.contains("error")) br.error        = o["error"].get<std::string>();
                r.orders.push_back(br);
            }
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
public:
    std::string              descr_order;
    std::vector<std::string> txids;
    std::optional<std::string> orig_txid;

    static EditOrderResult from_json(const json& j) {
        EditOrderResult r;
        if (j.contains("descr")) r.descr_order = j["descr"].value("order", "");
        if (j.contains("txid"))  r.txids        = j["txid"].get<std::vector<std::string>>();
        if (j.contains("originaltxid")) r.orig_txid = j["originaltxid"].get<std::string>();
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
public:
    std::string amend_id;
    static AmendOrderResult from_json(const json& j) {
        AmendOrderResult r;
        r.amend_id = j.value("amend_id", "");
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
public:
    int32_t count{0};   // number of orders cancelled
    bool    pending{false};
    static CancelOrderResult from_json(const json& j) {
        CancelOrderResult r;
        r.count   = j.value("count", 0);
        r.pending = j.value("pending", false);
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
public:
    int32_t count{0};
    static CancelAllResult from_json(const json& j) {
        CancelAllResult r;
        r.count = j.value("count", 0);
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
public:
    std::string current_time;
    std::string trigger_time;
    static CancelAllAfterResult from_json(const json& j) {
        CancelAllAfterResult r;
        r.current_time = j.value("currentTime", "");
        r.trigger_time = j.value("triggerTime", "");
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
public:
    int32_t count{0};
    static CancelOrderBatchResult from_json(const json& j) {
        CancelOrderBatchResult r;
        r.count = j.value("count", 0);
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
public:
    std::string token;
    int64_t     expires{0};
    static WebSocketsTokenResult from_json(const json& j) {
        WebSocketsTokenResult r;
        r.token   = j.value("token", "");
        r.expires = j.value("expires", int64_t{0});
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
public:
    std::string method;
    std::string limit;
    std::string fee;
    bool        gen_address{false};
    static DepositMethod from_json(const json& j) {
        DepositMethod r;
        r.method      = j.value("method", "");
        r.limit       = j.value("limit", "");
        r.fee         = j.value("fee", "");
        r.gen_address = j.value("gen-address", false);
        return r;
    }
};

class DepositMethodsResult : public kraken::IRestResult {
public:
    std::vector<DepositMethod> methods;
    static DepositMethodsResult from_json(const json& j) {
        DepositMethodsResult r;
        for (const auto& m : j) r.methods.push_back(DepositMethod::from_json(m));
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
public:
    std::string address;
    std::string expiretm;
    bool        new_addr{false};
    static DepositAddress from_json(const json& j) {
        DepositAddress r;
        r.address  = j.value("address", "");
        r.expiretm = j.value("expiretm", "");
        r.new_addr = j.value("new", false);
        return r;
    }
};

class DepositAddressesResult : public kraken::IRestResult {
public:
    std::vector<DepositAddress> addresses;
    static DepositAddressesResult from_json(const json& j) {
        DepositAddressesResult r;
        for (const auto& a : j) r.addresses.push_back(DepositAddress::from_json(a));
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
public:
    std::string refid;
    static WithdrawResult from_json(const json& j) {
        WithdrawResult r;
        r.refid = j.value("refid", "");
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
public:
    bool result{false};
    static CancelWithdrawalResult from_json(const json& j) {
        CancelWithdrawalResult r;
        r.result = j.get<bool>();
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
public:
    bool result{false};
    static CreateSubaccountResult from_json(const json& j) {
        CreateSubaccountResult r;
        r.result = j.get<bool>();
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
public:
    bool result{false};
    static EarnBoolResult from_json(const json& j) {
        EarnBoolResult r;
        r.result = j.get<bool>();
        return r;
    }
};

} // namespace kraken::rest
