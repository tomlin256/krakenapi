// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include "exchange/common/tick_price.hpp"

#include <benchmark/benchmark.h>

using exchange::TickPrice;

namespace {

// Candidate #6: small, but called once per price field on every hot message
// if a caller re-derives it (docs/plans/025 Design decisions).

void BM_TickPrice_From(benchmark::State& state) {
    for (auto _ : state) {
        auto tp = TickPrice::from(71770.4, 4);
        benchmark::DoNotOptimize(tp);
    }
}
BENCHMARK(BM_TickPrice_From);

void BM_TickPrice_Str(benchmark::State& state) {
    const auto tp = TickPrice::from(71770.4, 4);
    for (auto _ : state) {
        auto s = tp.str();
        benchmark::DoNotOptimize(s);
    }
}
BENCHMARK(BM_TickPrice_Str);

} // namespace
