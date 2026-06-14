// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================
//
// Compile + link + behavioural proof for the deprecated *source*-compat shim
// (src/kraken_ws_client.cpp and src/kraken_rest_client.cpp). It is the
// source-level counterpart to test_compat_shim.cpp (which proves the *header*
// shim).
//
// Pre-refactor consumers (notably flywheel) do not link krakenapi's CMake
// targets; they build their own WS-only and REST-only static libraries by
// compiling those two legacy source paths directly, then link both into one
// program. The refactor relocated/split the implementations, so each legacy
// path is now a thin unity TU that #includes the relocated source(s).
//
// This test target reproduces that exact consumer build (see
// tests/unit/CMakeLists.txt): compat_ws_shim is built from kraken_ws_client.cpp,
// compat_rest_shim from kraken_rest_client.cpp, and this executable links BOTH
// — and NOT krakenapi/exchange_common/exchange_http, which already contain the
// same TUs. So it guards two regressions a header-only test cannot:
//   - a relocated TU that moved again (→ this fails to compile/link), and
//   - tick_price.cpp landing in both shim libs (→ duplicate-symbol link error,
//     the failure mode that would hit a consumer linking both libraries).
//
// Built only when KRAKENAPI_BUILD_KRAKEN AND KRAKENAPI_BUILD_COMPAT_SHIM, with
// KRAKENAPI_SUPPRESS_DEPRECATION so the forwarders' #pragma message stays quiet.

// Old-path forwarders — reached exactly as a pre-refactor consumer reaches them.
#include "kraken_ws_client.hpp"
#include "kraken_ws_api.hpp"
#include "kraken_rest_client.hpp"
#include "kraken_rest_api.hpp"

#include "mock_ws_connection.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

// ── WS path: symbols come from compat_ws_shim (kraken_ws_client.cpp) ──────────
// Builds a client through the deprecated conn-form forwarder and sends a frame,
// exercising ExchangeWsClient's out-of-line methods — the ones the WS shim TU
// must supply for a consumer that never links krakenapi proper.
TEST(CompatSourceShim, WsClientLinksAndSends) {
    auto conn   = std::make_shared<MockWsConnection>();
    auto client = kraken::ws::make_ws_client(conn);  // [[deprecated]] conn forwarder
    ASSERT_NE(client, nullptr);

    conn->fire_open();
    client->execute_async(kraken::ws::PingRequest{});

    ASSERT_FALSE(conn->sent_messages.empty());
    EXPECT_NE(conn->sent_messages.back().find("ping"), std::string::npos);
}

// ── REST path: symbols come from compat_rest_shim (kraken_rest_client.cpp) ────
// Round-trips a request through a mock HTTP performer, exercising
// KrakenRestClient::execute (kraken/rest_client.cpp) and the shared transport —
// the symbols the REST shim TU must supply.
TEST(CompatSourceShim, RestClientLinksAndRoundTrips) {
    auto client = kraken::rest::make_test_client(
        [](const kraken::rest::HttpRequest& http) {
            EXPECT_EQ(http.path, "/0/public/Time");
            return R"({"error":[],"result":{"unixtime":1700000000,)"
                   R"("rfc1123":"Mon, 14 Nov 2023 00:00:00 +0000"}})";
        });

    const auto resp = client.execute(kraken::rest::GetServerTimeRequest{});
    ASSERT_TRUE(resp.ok);
    ASSERT_TRUE(resp.result.has_value());
    EXPECT_EQ(resp.result->unixtime, 1700000000);
}
