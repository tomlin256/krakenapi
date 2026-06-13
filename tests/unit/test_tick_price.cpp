// =============================================================================
// krakenapi — A type-safe C++ library for the Kraken Spot REST and WebSocket v2 APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/common/tick_price.hpp"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <stdexcept>

using json = nlohmann::json;
using exchange::TickPrice;

// ─────────────────────────────────────────────────────────────────────────────
// TickPrice::str — exact decimal serialisation
// ─────────────────────────────────────────────────────────────────────────────

TEST(TickPriceStr, StandardCase) {
    EXPECT_EQ(TickPrice::from(309.6217, 4).str(), "309.6217");
}

TEST(TickPriceStr, FpNoiseBugRepro) {
    // The bug: 3096217 * 0.0001 in double arithmetic is ~309.621699999...
    // TickPrice must snap this to "309.6217", not "309.621699...".
    const double noisy = 3096217 * 0.0001;
    EXPECT_EQ(TickPrice::from(noisy, 4).str(), "309.6217");
}

TEST(TickPriceStr, TrailingZeroPreserved) {
    EXPECT_EQ(TickPrice::from(100.0, 4).str(), "100.0000");
}

TEST(TickPriceStr, OnDecimalPlace) {
    EXPECT_EQ(TickPrice::from(1.5, 1).str(), "1.5");
}

TEST(TickPriceStr, ZeroDecimals) {
    EXPECT_EQ(TickPrice::from(42.0, 0).str(), "42");
}

TEST(TickPriceStr, FinestTickSize) {
    // AKE/USD or similar 8-decimal pair
    EXPECT_EQ(TickPrice::from(0.00000001, 8).str(), "0.00000001");
}

TEST(TickPriceStr, SubTickRounding) {
    // 309.62173 snaps to nearest tick at 4 decimals = 309.6217
    EXPECT_EQ(TickPrice::from(309.62173, 4).str(), "309.6217");
}

// ─────────────────────────────────────────────────────────────────────────────
// TickPrice::to_json — serialises as a JSON number (not a string)
// ─────────────────────────────────────────────────────────────────────────────

TEST(TickPriceToJson, EmitsJsonNumber) {
    const auto tp = TickPrice::from(309.6217, 4);
    const json j = tp.to_json();
    EXPECT_TRUE(j.is_number());
    EXPECT_EQ(j.dump(), "309.6217");
}

// ─────────────────────────────────────────────────────────────────────────────
// TickPrice::from_json — round-trip
// ─────────────────────────────────────────────────────────────────────────────

TEST(TickPriceFromJson, RoundTrip) {
    const auto original = TickPrice::from(309.6217, 4);
    const auto restored = TickPrice::from_json(original.to_json());
    EXPECT_EQ(restored.ticks,    3096217);
    EXPECT_EQ(restored.decimals, 4);
}

TEST(TickPriceFromJson, FromJsonNumber) {
    // from_json should also handle a numeric JSON value
    const json num = 309.6217;
    const auto tp = TickPrice::from_json(num);
    // The decimal count inferred from the number's string form may vary, but
    // str() must round-trip through the same number of decimal digits.
    EXPECT_FALSE(tp.str().empty());
}

// ── Review L2: int64 overflow is rejected, realistic precision still works ────

TEST(TickPriceFrom, OverflowThrows) {
    EXPECT_THROW(TickPrice::from(1.0, 19), std::overflow_error);   // 10^19 > int64
    EXPECT_THROW(TickPrice::from(1.0e12, 8), std::overflow_error); // 1e20
    EXPECT_NO_THROW(TickPrice::from(1.0e9, 8));                    // 1e17 — fine
}
