// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_ix_ws_connection.hpp — thin compatibility shim
//
// IxWsConnection has moved to exchange/common/ix_ws_connection.hpp.
// The URL-based make_ws_client() overload below delegates to the generic
// make_exchange_ws_client() with the Kraken frame descriptor.
//
// Usage (unchanged from pre-refactor code):
//   #include "kraken_ix_ws_connection.hpp"
//   auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

#include "exchange/common/ix_ws_connection.hpp"
#include "kraken_ws_client.hpp"

namespace kraken::ws {

// Re-export IxWsConnection into the Kraken namespace for pre-refactor callers.
using exchange::ws::IxWsConnection;

// URL-based factory: creates a fresh IxWsConnection, calls init() + connect(),
// and wires the Kraken frame descriptor.
inline std::shared_ptr<KrakenWsClient>
make_ws_client(const std::string&               url,
               std::shared_ptr<IWsErrorHandler>  error_handler = nullptr) {
    return exchange::ws::make_exchange_ws_client(
        url,
        kraken_frame_descriptor,
        std::move(error_handler));
}

} // namespace kraken::ws
