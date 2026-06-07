// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/kraken/types.hpp"

#include <gtest/gtest.h>

using exchange::OrderType;

namespace {

// All ten OrderType values, paired with the wire string each converter must
// produce. The generic converter (shared across exchanges) uses underscores;
// Kraken's wire format uses hyphens for the same multi-word values — these
// are genuinely different strings, not a naming-convention nuance.
struct Case {
    OrderType   value;
    const char* generic;
    const char* kraken;
};

constexpr Case kCases[] = {
    {OrderType::Limit,             "limit",              "limit"},
    {OrderType::Market,            "market",             "market"},
    {OrderType::Iceberg,           "iceberg",            "iceberg"},
    {OrderType::StopLoss,          "stop_loss",          "stop-loss"},
    {OrderType::StopLossLimit,     "stop_loss_limit",    "stop-loss-limit"},
    {OrderType::TakeProfit,        "take_profit",        "take-profit"},
    {OrderType::TakeProfitLimit,   "take_profit_limit",  "take-profit-limit"},
    {OrderType::TrailingStop,      "trailing_stop",      "trailing-stop"},
    {OrderType::TrailingStopLimit, "trailing_stop_limit","trailing-stop-limit"},
    {OrderType::SettlePosition,    "settle_position",    "settle-position"},
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// exchange::to_string(OrderType) — generic, underscore-separated wire format
// ─────────────────────────────────────────────────────────────────────────────

TEST(OrderTypeToString, GenericUsesUnderscores) {
    for (const auto& c : kCases)
        EXPECT_EQ(exchange::to_string(c.value), c.generic);
}

// ─────────────────────────────────────────────────────────────────────────────
// kraken_order_type_to_string / kraken_order_type_from_string — Kraken's
// hyphen-separated wire format (byte-identical to the pre-refactor kraken::
// converters; this is what the compat shim must forward to).
// ─────────────────────────────────────────────────────────────────────────────

TEST(OrderTypeToString, KrakenUsesHyphens) {
    for (const auto& c : kCases)
        EXPECT_EQ(exchange::kraken::kraken_order_type_to_string(c.value), c.kraken);
}

TEST(OrderTypeToString, GenericAndKrakenDivergeForMultiWordValues) {
    // The whole reason kraken_order_type_to_string exists as a distinct
    // function: for any multi-word value, the generic and Kraken strings
    // are NOT the same. A shim that re-exported the generic overload for
    // OrderType would silently corrupt Kraken's wire format.
    for (const auto& c : kCases) {
        const std::string generic = exchange::to_string(c.value);
        const std::string kraken  = exchange::kraken::kraken_order_type_to_string(c.value);
        if (generic.find('_') != std::string::npos)
            EXPECT_NE(generic, kraken) << "expected divergence for " << generic;
        else
            EXPECT_EQ(generic, kraken) << "single-word values should match";
    }
}

TEST(OrderTypeFromString, KrakenRoundTrip) {
    for (const auto& c : kCases)
        EXPECT_EQ(exchange::kraken::kraken_order_type_from_string(c.kraken), c.value);
}

TEST(OrderTypeFromString, ThrowsOnUnknown) {
    EXPECT_THROW(exchange::kraken::kraken_order_type_from_string("not-a-real-type"),
                 std::invalid_argument);
}
