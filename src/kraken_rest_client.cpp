// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

// kraken_rest_client.cpp — DEPRECATED source-compatibility shim.
//
// The multi-exchange refactor split the old single REST translation unit into
// three: the Kraken REST client (kraken/rest_client.cpp), the shared libcurl
// transport (exchange/common/http_client.cpp, plan 010), and the exact-decimal
// TickPrice helper (exchange/common/tick_price.cpp, plan 009).
//
// Some pre-refactor consumers do not link krakenapi's CMake targets; instead
// they build their own REST-only static library by compiling this *exact source
// path* directly, e.g.:
//
//     add_library(krakenapi_rest STATIC ${krakenapi_SOURCE_DIR}/src/kraken_rest_client.cpp)
//
// This file preserves that path by compiling all three relocated translation
// units, so the legacy single source still yields a complete REST library. It
// is the source-level counterpart to the header shim (kraken_compat.hpp + the
// kraken_*.hpp forwarders).
//
// Deliberately NOT listed in src/CMakeLists.txt: krakenapi's own build compiles
// these three TUs directly (via the krakenapi / exchange_http / exchange_common
// targets), and compiling these copies too would duplicate their symbols. This
// file exists solely for external builds that hardcode the legacy path.
//
// tick_price.cpp lives here and NOT in the WS shim (kraken_ws_client.cpp):
// TickPrice::from_json is its only out-of-line symbol and the WS path never
// calls it, so a consumer that links both shim libraries gets no duplicate
// symbols.
//
// #include of .cpp files is intentional (a unity translation unit): it lets one
// hardcoded source path yield several relocated TUs' object code while
// delegating to the real sources, so this shim cannot drift from them.

// NOLINTBEGIN(bugprone-suspicious-include) — intentional unity TU; see above.
#include "kraken/rest_client.cpp"
#include "exchange/common/http_client.cpp"
#include "exchange/common/tick_price.cpp"
// NOLINTEND(bugprone-suspicious-include)
