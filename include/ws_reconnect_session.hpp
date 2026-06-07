// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// ws_reconnect_session.hpp — DEPRECATED: use exchange/common/reconnect_session.hpp instead.
//
// This header is a thin compatibility forwarder. It pulls in the new-layout
// implementation and reopens the legacy kraken::ws:: namespace on top of it
// via kraken_compat.hpp, so pre-refactor includes keep compiling unchanged.

#ifndef KRAKENAPI_SUPPRESS_DEPRECATION
#  pragma message("ws_reconnect_session.hpp is deprecated; include exchange/common/reconnect_session.hpp. See docs/plans/001-appendix-migration-guide.md (removed in vNEXT_MAJOR).")
#endif

#include "exchange/common/reconnect_session.hpp"
#include "kraken_compat.hpp"
