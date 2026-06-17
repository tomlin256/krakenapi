// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/kraken/ws_client.hpp"

#include <memory>
#include <utility>

namespace exchange::kraken::ws {

// Wraps an already-managed IWsConnection with the Kraken frame descriptor.
std::shared_ptr<KrakenWsClient>
make_kraken_ws_client(std::shared_ptr<IWsConnection>   conn,
                      std::shared_ptr<IWsErrorHandler>  error_handler) {
    return exchange::ws::make_exchange_ws_client(
        std::move(conn),
        kraken_frame_descriptor,
        std::move(error_handler));
}

} // namespace exchange::kraken::ws
