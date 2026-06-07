// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// ws_reconnect_session.hpp — thin compatibility shim
//
// WsReconnectSession has moved to exchange/common/reconnect_session.hpp.
// This file re-exports it into the kraken::ws namespace for callers that
// include the old path.

#include "exchange/common/reconnect_session.hpp"

namespace kraken::ws {

using exchange::ws::WsReconnectSession;

} // namespace kraken::ws
