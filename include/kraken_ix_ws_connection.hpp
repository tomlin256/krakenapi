// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_ix_ws_connection.hpp — DEPRECATED: use exchange/common/ix_ws_connection.hpp
// (for IxWsConnection) and exchange/kraken/ws_client.hpp (for the Kraken frame
// descriptor) instead.
//
// Unlike the other six forwarders, this header retains one real definition:
// the URL-based make_ws_client() overload. It cannot move into kraken_compat.hpp
// — that header is reachable from pure-REST entry points (e.g.
// kraken_rest_client.hpp), and constructing a client straight from a URL string
// requires IxWsConnection, which requires linking ixwebsocket. Keeping it here
// preserves the invariant that libkrakenapi.a does not require ixwebsocket.
//
// Usage (unchanged from pre-refactor code):
//   #include "kraken_ix_ws_connection.hpp"
//   auto client = kraken::ws::make_ws_client(std::string(kraken::ws::PUBLIC_WS_URL));

#ifndef KRAKENAPI_SUPPRESS_DEPRECATION
#  pragma message("kraken_ix_ws_connection.hpp is deprecated; include exchange/common/ix_ws_connection.hpp and exchange/kraken/ws_client.hpp. See docs/plans/001-appendix-migration-guide.md (removed in vNEXT_MAJOR).")
#endif

#include "exchange/common/ix_ws_connection.hpp"
#include "exchange/kraken/ws_client.hpp"
#include "kraken_compat.hpp"

namespace kraken::ws {

using exchange::ws::IxWsConnection;

// URL-based factory: creates a fresh IxWsConnection, calls init() + connect(),
// and wires the Kraken frame descriptor. Delegates to the real underlying
// factory (exchange::ws::make_exchange_ws_client's URL overload) rather than
// replicating its body, so the shim cannot drift from real behaviour.
[[deprecated("use exchange::ws::make_exchange_ws_client(url, exchange::kraken::ws::kraken_frame_descriptor)")]]
inline std::shared_ptr<exchange::ws::ExchangeWsClient>
make_ws_client(const std::string&                              url,
               std::shared_ptr<exchange::ws::IWsErrorHandler>  error_handler = nullptr)
{
    return exchange::ws::make_exchange_ws_client(
        url, exchange::kraken::ws::kraken_frame_descriptor, std::move(error_handler));
}

} // namespace kraken::ws
