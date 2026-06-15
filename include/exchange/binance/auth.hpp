// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/binance/auth.hpp
// Binance REST authentication: BinanceCredentials, BinanceAuth (IRestAuth implementor),
// and supporting HMAC-SHA256 / hex-encoding utilities.
//
// Namespace: exchange::binance::rest

#include "exchange/common/rest.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

// OpenSSL headers — required for HMAC-SHA256.
#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace exchange::binance::rest {

// ── Crypto utilities ─────────────────────────────────────────────────────────

namespace detail {

// HMAC-SHA256 of `data` using `key`, returning raw bytes.
// Key material is the raw UTF-8 secret bytes (unlike Kraken, which base64-decodes first).
inline std::string hmac_sha256(const std::string& key, const std::string& data) {
    unsigned char result[32];
    unsigned int  len = 32;
    if (HMAC(EVP_sha256(),
             reinterpret_cast<const unsigned char*>(key.data()), static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char*>(data.data()), data.size(),
             result, &len) == nullptr)        // review L4: surface OpenSSL failure
        throw std::runtime_error("HMAC-SHA256 computation failed");
    return std::string(reinterpret_cast<char*>(result), len);
}

// Encode raw bytes as a lowercase hex string.
inline std::string to_hex(const std::string& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char c : bytes)
        oss << std::setw(2) << static_cast<unsigned int>(c);
    return oss.str();
}

} // namespace detail

// ── Signing algorithm selector ────────────────────────────────────────────────

enum class BinanceSignAlgorithm { HmacSha256, Rsa, Ed25519 };

// ── Credential bundle ────────────────────────────────────────────────────────

struct BinanceCredentials {
    std::string          api_key;
    std::string          secret_key;
    BinanceSignAlgorithm algorithm{BinanceSignAlgorithm::HmacSha256};
    // Included in every signed request.  Binance default is 5000 ms; max 60000.
    int                  recv_window_ms{5000};
};

// ── IRestAuth implementor ─────────────────────────────────────────────────────
//
// BinanceAuth wraps BinanceCredentials and implements IRestAuth::sign().
//
// Signing differences from Kraken (Option 2 pattern — see Step 3 notes in the plan):
//   Digest      : HMAC-SHA256  (Kraken uses HMAC-SHA512)
//   Signed payload: query_string + body (already URL-encoded, raw concatenation)
//   Output encoding: lowercase hex  (Kraken uses base64)
//   Anti-replay : timestamp (ms) + recvWindow injected here  (Kraken embeds nonce in build())
//   Key material: raw UTF-8 secret bytes  (Kraken base64-decodes)
//
// sign() appends "timestamp=<ms>&recvWindow=<ms>" to the query string (GET/DELETE)
// or request body (POST), then computes the signature over the full
// (query + body) concatenation and appends "&signature=<hex>" to the same field.
// The X-MBX-APIKEY header is also injected.
//
// The ClockFn parameter makes the timestamp injectable for unit tests.
struct BinanceAuth : exchange::rest::IRestAuth {
    using ClockFn = std::function<int64_t()>;

    explicit BinanceAuth(BinanceCredentials creds, ClockFn clock = {})
        : creds_(std::move(creds))
        , clock_(clock ? std::move(clock) : []() -> int64_t {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
          })
    {}

    void sign(exchange::rest::HttpRequest& req) const override {
        // Only HMAC-SHA256 is implemented. Reject other algorithms loudly rather
        // than silently HMAC-signing with the wrong scheme (would produce a
        // signature the server rejects, with no clue why).
        if (creds_.algorithm != BinanceSignAlgorithm::HmacSha256)
            throw std::invalid_argument(
                "BinanceAuth: only HMAC-SHA256 signing is implemented "
                "(RSA / Ed25519 are not supported)");

        using M = exchange::rest::HttpRequest::Method;

        const bool is_body_request = (req.method == M::POST);

        // Build the timestamp + recvWindow suffix.
        std::string ts_params = "timestamp=" + std::to_string(clock_());
        if (creds_.recv_window_ms > 0)
            ts_params += "&recvWindow=" + std::to_string(creds_.recv_window_ms);

        // Append to the appropriate field (query for GET/DELETE, body for POST).
        if (is_body_request) {
            if (!req.body.empty()) req.body += '&';
            req.body += ts_params;
            if (req.headers.find("Content-Type") == req.headers.end())
                req.headers["Content-Type"] = "application/x-www-form-urlencoded";
        } else {
            if (!req.query.empty()) req.query += '&';
            req.query += ts_params;
        }

        // Signature covers the full "queryString + requestBody" concatenation.
        std::string payload = req.query + req.body;
        std::string sig = detail::to_hex(detail::hmac_sha256(creds_.secret_key, payload));

        if (is_body_request)
            req.body += "&signature=" + sig;
        else
            req.query += "&signature=" + sig;

        req.headers["X-MBX-APIKEY"] = creds_.api_key;
    }

private:
    BinanceCredentials creds_;
    ClockFn            clock_;
};

} // namespace exchange::binance::rest
