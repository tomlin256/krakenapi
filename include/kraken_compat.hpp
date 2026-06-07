// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// kraken_compat.hpp
//
// Deprecated compatibility shim. Reopens the old `kraken::` namespace on top
// of the new `exchange::kraken::` layout so that code written against the
// pre-refactor API keeps compiling and behaving identically.
//
// This header is included transitively by every old-path forwarder header
// (kraken_types.hpp, kraken_rest_api.hpp, …) and is therefore self-sufficient:
// it #includes everything it needs rather than relying on whichever entry
// point happened to be included first.
//
// Deliberately ixwebsocket-free: URL-based WebSocket construction and
// IxWsConnection remain solely in kraken_ix_ws_connection.hpp, preserving the
// invariant that libkrakenapi.a does not require linking against ixwebsocket.

#include "exchange/kraken/types.hpp"
#include "exchange/kraken/auth.hpp"
#include "exchange/kraken/rest_api.hpp"
#include "exchange/kraken/rest_client.hpp"
#include "exchange/kraken/ws_api.hpp"
#include "exchange/kraken/ws_client.hpp"
#include "exchange/common/reconnect_session.hpp"

namespace kraken {

// ── Shared enum type re-exports ──────────────────────────────────────────────
// Side, TimeInForce, and OrderStatus have wire formats identical between the
// generic exchange:: converters and Kraken's old converters — only OrderType
// diverges (Kraken uses hyphens; the generic converter uses underscores).

using exchange::Side;
using exchange::OrderType;
using exchange::TimeInForce;
using exchange::OrderStatus;

// ── Kraken-specific enum type re-exports ─────────────────────────────────────
// These have no generic exchange:: counterpart — they are Kraken-only concepts.

using exchange::kraken::PriceType;
using exchange::kraken::TriggerReference;
using exchange::kraken::StpType;
using exchange::kraken::FeePreference;

// ── to_string / from_string forwarders ───────────────────────────────────────
//
// Deliberately NOT a blanket `using exchange::to_string;` or
// `using exchange::kraken::to_string;` — either would pull the generic,
// underscore-separated OrderType overload ("stop_loss") into this overload
// set alongside (or instead of) Kraken's hyphenated wire format
// ("stop-loss"), silently corrupting requests built through the old API.
// exchange::kraken::to_string itself has this problem transitively (it has
// its own `using exchange::to_string;`), so even forwarding that whole
// overload set is unsafe. Each enum gets its own explicitly-typed forwarder
// naming the exact converter that reproduces the old kraken:: byte format.

inline std::string to_string(Side v) { return exchange::to_string(v); }
inline Side side_from_string(const std::string& s) { return exchange::side_from_string(s); }

inline std::string to_string(OrderType v) { return exchange::kraken::kraken_order_type_to_string(v); }
inline OrderType order_type_from_string(const std::string& s) { return exchange::kraken::kraken_order_type_from_string(s); }

inline std::string to_string(TimeInForce v) { return exchange::to_string(v); }
inline TimeInForce tif_from_string(const std::string& s) { return exchange::kraken::kraken_tif_from_string(s); }

inline std::string to_string(OrderStatus v) { return exchange::to_string(v); }
inline OrderStatus order_status_from_string(const std::string& s) { return exchange::order_status_from_string(s); }

inline std::string to_string(PriceType v) { return exchange::kraken::to_string(v); }
inline PriceType price_type_from_string(const std::string& s) { return exchange::kraken::price_type_from_string(s); }

inline std::string to_string(TriggerReference v) { return exchange::kraken::to_string(v); }
inline TriggerReference trigger_ref_from_string(const std::string& s) { return exchange::kraken::trigger_ref_from_string(s); }

inline std::string to_string(StpType v) { return exchange::kraken::to_string(v); }
inline StpType stp_type_from_string(const std::string& s) { return exchange::kraken::stp_type_from_string(s); }

inline std::string to_string(FeePreference v) { return exchange::kraken::to_string(v); }
inline FeePreference fee_preference_from_string(const std::string& s) { return exchange::kraken::fee_preference_from_string(s); }

// ── Kraken-specific struct re-exports ─────────────────────────────────────────

using exchange::kraken::TickPrice;
using exchange::kraken::Triggers;
using exchange::kraken::Conditional;
using exchange::kraken::OrderParams;
using exchange::kraken::OrderDescription;
using exchange::kraken::OrderInfo;
using exchange::kraken::TradeInfo;
using exchange::kraken::LedgerEntry;

// ── Generic REST response envelope ───────────────────────────────────────────
// This lived at kraken:: scope (not kraken::rest::) in the old layout, and its
// new home is exchange::kraken:: — NOT exchange::rest::, which is a distinct,
// differently-shaped type that would silently mismatch old call sites.

using exchange::kraken::RestResponse;
using exchange::kraken::parse_rest_response;

namespace rest {
using namespace exchange::kraken::rest;

// Deliberately NOT also writing `using exchange::rest::RestResponse;` here:
// kraken::rest::RestResponse never existed pre-refactor (RestResponse lived
// only at kraken:: scope — see the re-export above) — adding it would invent
// a new qualified path pointing at a differently-shaped generic type.
// HttpRequest needs no separate re-export either: exchange::kraken::rest_api.hpp
// already brings exchange::rest::HttpRequest in via `using`, so the `using
// namespace` above carries it across — the very type the old kraken::rest::
// HttpRequest was relocated to become.

} // namespace rest

namespace ws {
using namespace exchange::kraken::ws;

// exchange::kraken::ws re-exports 7 of the 9 types the old kraken::ws shim
// carried; FrameDescriptor and FrameKind didn't make the trip. Old code that
// names them directly (e.g. to write a custom FrameDescriptor) must keep
// compiling, so they're re-exported here from their canonical home.
using exchange::ws::FrameDescriptor;
using exchange::ws::FrameKind;

// WsReconnectSession is generic common-scaffold code (exchange::ws::), but the
// old ws_reconnect_session.hpp shim re-exported it into kraken::ws:: too —
// preserved here so old code naming kraken::ws::WsReconnectSession compiles.
using exchange::ws::WsReconnectSession;

using KrakenWsClient [[deprecated("use exchange::ws::ExchangeWsClient")]]
    = exchange::ws::ExchangeWsClient;

// Connection-based factory — relocated here from the old kraken_ws_client.hpp
// (consolidating the two make_ws_client overloads' homes into one source of
// truth). Delegates to the canonical new entry point rather than
// re-implementing it, so the shim cannot drift from real behaviour.
[[deprecated("use exchange::kraken::ws::make_kraken_ws_client")]]
inline std::shared_ptr<exchange::ws::ExchangeWsClient>
make_ws_client(std::shared_ptr<exchange::ws::IWsConnection>   conn,
               std::shared_ptr<exchange::ws::IWsErrorHandler> error_handler = nullptr)
{
    return exchange::kraken::ws::make_kraken_ws_client(std::move(conn), std::move(error_handler));
}

// NOTE: the URL-based make_ws_client(url) overload and IxWsConnection are
// deliberately NOT re-exported here — both require linking ixwebsocket, and
// this header is reachable from pure-REST entry points (e.g.
// kraken_rest_client.hpp). They remain solely in kraken_ix_ws_connection.hpp.

} // namespace ws

} // namespace kraken
