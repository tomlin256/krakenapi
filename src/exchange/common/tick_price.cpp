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
#include <string>

namespace exchange {

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
