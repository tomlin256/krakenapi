// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#pragma once

// exchange/common/tick_price.hpp
// TickPrice — exact-decimal price representation, exchange-agnostic.
//
// Stores a price as an integer tick count plus a decimal-point position, so the
// decimal string is built by integer point-insertion (no floating-point
// formatting, no locale). Generic: any adapter that needs exact-decimal price
// round-tripping can use it. Re-exported into adapter namespaces (e.g.
// exchange::kraken::TickPrice) with a using declaration.
//
// Namespace: exchange

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace exchange {

using json = nlohmann::json;

struct TickPrice {
    int64_t ticks{0};
    int     decimals{0};

    // Snap an absolute price onto the tick grid for a given decimal precision.
    static TickPrice from(double price, int decimals) {
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
    std::string str() const {
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

    json to_json() const { return std::stod(str()); }

    static TickPrice from_json(const json& j);
};

} // namespace exchange
