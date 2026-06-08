// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/kraken/auth.hpp
// Kraken REST authentication: Credentials, KrakenAuth (IRestAuth implementor),
// nonce generator, and supporting crypto utilities.
//
// Namespace: exchange::kraken::rest

#include "exchange/common/rest.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

// OpenSSL headers — required for sign().
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace exchange::kraken::rest {

// ── Crypto utilities ─────────────────────────────────────────────────────────

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
        unsigned int val = static_cast<unsigned char>(data[i]) << 16;
        if (i + 1 < len) val |= static_cast<unsigned char>(data[i+1]) << 8;
        if (i + 2 < len) val |= static_cast<unsigned char>(data[i+2]);
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
            oss << '%' << std::uppercase << std::hex << std::setw(2)
                << std::setfill('0') << static_cast<unsigned int>(c);
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

// ── Nonce helper ─────────────────────────────────────────────────────────────

// Returns a monotonically increasing nonce based on the system clock.
// Uses microseconds to match the KAPI nonce scale (16-digit numbers).
inline uint64_t make_nonce() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
}

// ── Auth credential bundle ───────────────────────────────────────────────────

struct Credentials {
    std::string api_key;     // public key  → API-Key header
    std::string api_secret;  // base64-encoded private key → used for signing

    static Credentials from_file(const std::string& name,
                                 const std::string& location = "") {
        std::string dir;
        if (location.empty()) {
            const char* home = getenv("HOME");
            if (!home) throw std::runtime_error("HOME environment variable not set");
            dir = std::string(home) + "/.kraken";
        } else {
            dir = location;
        }

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

// ── IRestAuth implementor ─────────────────────────────────────────────────────
//
// KrakenAuth wraps Credentials and implements IRestAuth::sign().
//
// Design note (Option 2, Step 3): PrivateRequest::build() constructs the
// complete request body — including the nonce field — before calling sign().
// sign() therefore extracts the nonce from the already-built body rather than
// injecting it. This keeps build() shapes stable and confines all header
// injection to a single chokepoint.
//
// Two body formats are supported:
//   application/x-www-form-urlencoded — nonce= appears as the first key-value pair
//   application/json                  — nonce is a top-level string field
struct KrakenAuth : exchange::rest::IRestAuth {
    explicit KrakenAuth(Credentials creds) : creds_(std::move(creds)) {}

    void sign(exchange::rest::HttpRequest& req) const override {
        std::string nonce_str;
        auto ct = req.headers.find("Content-Type");
        if (ct != req.headers.end() && ct->second == "application/json") {
            nonce_str = nlohmann::json::parse(req.body).at("nonce").get<std::string>();
        } else {
            const std::string prefix = "nonce=";
            auto pos = req.body.find(prefix);
            if (pos != std::string::npos) {
                pos += prefix.size();
                auto end = req.body.find('&', pos);
                nonce_str = req.body.substr(
                    pos, end == std::string::npos ? std::string::npos : end - pos);
            }
        }
        req.headers["API-Key"]  = creds_.api_key;
        req.headers["API-Sign"] = creds_.sign(req.path, nonce_str, req.body);
    }

private:
    Credentials creds_;
};

} // namespace exchange::kraken::rest
