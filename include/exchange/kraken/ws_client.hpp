// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/kraken/ws_client.hpp
// Kraken WebSocket client — Kraken URL constants and connection-based factory.
//
// KrakenWsClient is a type alias for exchange::ws::ExchangeWsClient,
// parameterised with the Kraken frame descriptor.
//
// Namespace: exchange::kraken::ws

#include "exchange/common/ws_client.hpp"
#include "exchange/kraken/ws_api.hpp"

namespace exchange::kraken::ws {

// ── Re-export common client types ────────────────────────────────────────────

using exchange::ws::IWsConnection;
using exchange::ws::IWsErrorHandler;
using exchange::ws::RateLimitedWsErrorHandler;
using exchange::ws::WsResponse;
using exchange::ws::SubscriptionHandle;
using exchange::ws::ExchangeWsClient;
using exchange::ws::MessageIdentifier;

// KrakenWsClient is ExchangeWsClient parameterised with Kraken's frame
// descriptor. Existing code holding shared_ptr<KrakenWsClient> continues to
// work because KrakenWsClient is the same type as ExchangeWsClient.
using KrakenWsClient = exchange::ws::ExchangeWsClient;

// ── Endpoint URL constants ────────────────────────────────────────────────────

inline constexpr std::string_view PUBLIC_WS_URL  = "wss://ws.kraken.com/v2";
inline constexpr std::string_view PRIVATE_WS_URL = "wss://ws-auth.kraken.com/v2";

// ── Connection-based factory ──────────────────────────────────────────────────
//
// Wraps an already-managed IWsConnection with the Kraken frame descriptor.
// Callers call conn->connect() themselves or use the URL overload in
// exchange/common/ix_ws_connection.hpp (which does it automatically).

inline std::shared_ptr<KrakenWsClient>
make_kraken_ws_client(std::shared_ptr<IWsConnection>   conn,
                      std::shared_ptr<IWsErrorHandler>  error_handler = nullptr) {
    return exchange::ws::make_exchange_ws_client(
        std::move(conn),
        kraken_frame_descriptor,
        std::move(error_handler));
}

} // namespace exchange::kraken::ws
