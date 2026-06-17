// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/common/tick_price.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace exchange {

// Snap an absolute price onto the tick grid for a given decimal precision.
TickPrice TickPrice::from(double price, int decimals) {
    const long double scaled =
        static_cast<long double>(price) * std::pow(10.0L, decimals);
    // Guard int64 overflow before llroundl/cast (UB otherwise, review L2).
    // 9.2e18 is comfortably below 2^63 and far above any real
    // price * 10^decimals (prices are bounded, decimals <= 8 on the wire).
    if (scaled > 9.2e18L || scaled < -9.2e18L)
        throw std::overflow_error(
            "TickPrice::from: price * 10^decimals overflows int64");
    return TickPrice{static_cast<int64_t>(std::llroundl(scaled)), decimals};
}

// Exact decimal string via integer point-insertion — no FP, no locale.
std::string TickPrice::str() const {
    const bool neg = ticks < 0;
    const uint64_t mag = neg
        ? static_cast<uint64_t>(-(ticks + 1)) + 1
        : static_cast<uint64_t>(ticks);
    std::string digits = std::to_string(mag);
    std::string out;
    if (decimals <= 0) {
        out = digits;
    } else {
        if (static_cast<int>(digits.size()) <= decimals)
            digits.insert(0, static_cast<size_t>(decimals) - digits.size() + 1, '0');
        const size_t dot = digits.size() - static_cast<size_t>(decimals);
        out = digits.substr(0, dot) + "." + digits.substr(dot);
    }
    return neg ? "-" + out : out;
}

json TickPrice::to_json() const { return std::stod(str()); }

TickPrice TickPrice::from_json(const json& j) {
    std::string s;
    if (j.is_string()) {
        s = j.get<std::string>();
    } else {
        // number: use json's serialised form (shortest round-trip representation)
        s = j.dump();
    }

    // Count digits after '.', if any.
    const auto dot = s.find('.');
    const int dec = (dot != std::string::npos)
        ? static_cast<int>(s.size() - dot - 1)
        : 0;

    // Parse value and snap to grid.
    const double val = std::stod(s);
    const long double scale = std::pow(10.0L, dec);
    const int64_t ticks = static_cast<int64_t>(
        std::llroundl(static_cast<long double>(val) * scale));
    return TickPrice{ticks, dec};
}

} // namespace exchange
