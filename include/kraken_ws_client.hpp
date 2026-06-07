// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_ws_client.hpp — DEPRECATED: use exchange/kraken/ws_client.hpp instead.
// thin compatibility shim
//
// KrakenWsClient is now a type alias for exchange::ws::ExchangeWsClient.
// All implementation logic lives in exchange/common/ws_client.hpp + .inl.
// The Kraken-specific MessageIdentifier (kraken_frame_descriptor) is defined
// in kraken_ws_api.hpp.
//
// Usage is unchanged from pre-refactor code:
//   auto client = kraken::ws::make_ws_client(conn);          // from conn
//   // or see kraken_ix_ws_connection.hpp for the URL-based overload.

#include "exchange/common/ws_client.hpp"
#include "kraken_ws_api.hpp"

namespace kraken::ws {

// ── Re-export common types ───────────────────────────────────────────────────

using exchange::ws::IWsConnection;
using exchange::ws::IWsErrorHandler;
using exchange::ws::RateLimitedWsErrorHandler;
using exchange::ws::WsResponse;
using exchange::ws::SubscriptionHandle;
using exchange::ws::ExchangeWsClient;
using exchange::ws::MessageIdentifier;
using exchange::ws::FrameDescriptor;
using exchange::ws::FrameKind;

// KrakenWsClient is the exchange-agnostic client parameterised with the
// Kraken frame descriptor.  Existing code that holds a
// std::shared_ptr<KrakenWsClient> continues to work because KrakenWsClient
// is the same type as ExchangeWsClient.
using KrakenWsClient = exchange::ws::ExchangeWsClient;

// ── Endpoint URL constants ───────────────────────────────────────────────────

inline constexpr std::string_view PUBLIC_WS_URL  = "wss://ws.kraken.com/v2";
inline constexpr std::string_view PRIVATE_WS_URL = "wss://ws-auth.kraken.com/v2";

// ── Connection-based factory ─────────────────────────────────────────────────
//
// Wraps an already-managed IWsConnection with the Kraken frame descriptor.
// Callers call conn->connect() themselves (or use the URL overload in
// kraken_ix_ws_connection.hpp which does it automatically).

inline std::shared_ptr<KrakenWsClient>
make_ws_client(std::shared_ptr<IWsConnection>   conn,
               std::shared_ptr<IWsErrorHandler>  error_handler = nullptr) {
    return exchange::ws::make_exchange_ws_client(
        std::move(conn),
        kraken_frame_descriptor,
        std::move(error_handler));
}

} // namespace kraken::ws
