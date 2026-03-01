// test_signature.cpp
//
// Proves that Credentials::sign() (kraken_rest_api.hpp) produces the same
// API-Sign header value as KAPI::signature() (kapi.cpp) for identical inputs.
//
// If any test here fails, the two signing implementations diverge and the
// new client will be rejected by Kraken with an invalid-signature error.

#include "kapi.hpp"
#include "kraken_rest_api.hpp"

#include <gtest/gtest.h>

#include <string>

// ============================================================
// Shared constants
// ============================================================

// A realistic-looking (but fake) 88-char base64 secret — same format that
// Kraken generates.  Any valid base64 blob works for these unit tests.
static const char* TEST_SECRET =
    "kQH5HW/8p1uGOVjbgWA7FunAmGO8lsSUXNsu3eow76sz84Q18fWxnyRzBHCd3pd5"
    "nE9qa99HAZtuZuj6F1huXg==";

// ============================================================
// Helpers
// ============================================================

static std::string kapi_sign(const std::string& secret,
                              const std::string& path,
                              const std::string& nonce,
                              const std::string& postdata)
{
    Kraken::KAPI kapi("dummy-key", secret);
    return kapi.signature(path, nonce, postdata);
}

static std::string new_sign(const std::string& secret,
                             const std::string& path,
                             const std::string& nonce,
                             const std::string& postdata)
{
    kraken::rest::Credentials creds{"dummy-key", secret};
    return creds.sign(path, nonce, postdata);
}

// ============================================================
// Tests
// ============================================================

// Balance (no extra params) — simplest possible private call.
TEST(SignatureEquivalence, Balance_NoExtraParams)
{
    const std::string path     = "/0/private/Balance";
    const std::string nonce    = "1616492376594";
    const std::string postdata = "nonce=1616492376594";

    EXPECT_EQ(kapi_sign(TEST_SECRET, path, nonce, postdata),
              new_sign (TEST_SECRET, path, nonce, postdata));
}

// GetWebSocketsToken — the actual call used in private_rest.cpp.
TEST(SignatureEquivalence, GetWebSocketsToken)
{
    const std::string path     = "/0/private/GetWebSocketsToken";
    const std::string nonce    = "1616492376594";
    const std::string postdata = "nonce=1616492376594";

    EXPECT_EQ(kapi_sign(TEST_SECRET, path, nonce, postdata),
              new_sign (TEST_SECRET, path, nonce, postdata));
}

// AddOrder — nonce first, then extra alphanumeric params (no URL-encoding needed).
TEST(SignatureEquivalence, AddOrder_AlphanumericParams)
{
    const std::string path     = "/0/private/AddOrder";
    const std::string nonce    = "1616492376594";
    const std::string postdata =
        "nonce=1616492376594"
        "&ordertype=limit&pair=XBTUSD&price=30000&type=buy&volume=1.25";

    EXPECT_EQ(kapi_sign(TEST_SECRET, path, nonce, postdata),
              new_sign (TEST_SECRET, path, nonce, postdata));
}

// Short secret — verifies that the trailing-zero quirk in KAPI's b64_decode
// (vector sized to input length, not decoded length) is harmless: both keys
// are < SHA-512 block size (128 bytes) so HMAC pads them to the same value.
TEST(SignatureEquivalence, ShortSecret_TrailingZerosDontAffectHMAC)
{
    // "dGVzdHNlY3JldA==" decodes to "testsecret" (10 bytes).
    const char* short_secret = "dGVzdHNlY3JldA==";
    const std::string path     = "/0/private/Balance";
    const std::string nonce    = "9999999999999";
    const std::string postdata = "nonce=9999999999999";

    EXPECT_EQ(kapi_sign(short_secret, path, nonce, postdata),
              new_sign (short_secret, path, nonce, postdata));
}
