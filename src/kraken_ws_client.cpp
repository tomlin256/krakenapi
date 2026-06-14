// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// kraken_ws_client.cpp — DEPRECATED source-compatibility shim.
//
// The multi-exchange refactor relocated the WebSocket client implementation to
// exchange/common/ws_client.cpp. Some pre-refactor consumers do not link
// krakenapi's CMake targets; instead they build their own WS-only static
// library by compiling this *exact source path* directly, e.g.:
//
//     add_library(krakenapi_ws STATIC ${krakenapi_SOURCE_DIR}/src/kraken_ws_client.cpp)
//
// This file preserves that path by compiling the relocated translation unit, so
// those consumers keep building unchanged. It is the source-level counterpart
// to the header shim (kraken_compat.hpp + the kraken_*.hpp forwarders).
//
// Deliberately NOT listed in src/CMakeLists.txt: krakenapi's own build compiles
// exchange/common/ws_client.cpp directly (via the exchange_common target), and
// compiling this copy too would duplicate ExchangeWsClient's symbols. This file
// exists solely for external builds that hardcode the legacy path.
//
// It pulls in only ws_client.cpp — not tick_price.cpp — because the WS engine
// never references TickPrice's one out-of-line symbol (TickPrice::from_json);
// that TU lives in the REST shim (kraken_rest_client.cpp) instead, so a consumer
// linking both shim libraries gets no duplicate symbols.
//
// #include of a .cpp is intentional (a unity translation unit): it lets one
// hardcoded source path yield the relocated implementation's object code while
// delegating to the real source, so this shim cannot drift from it.

// NOLINTNEXTLINE(bugprone-suspicious-include) — intentional unity TU; see above.
#include "exchange/common/ws_client.cpp"
