// =============================================================================
// cryptocogs — A type-safe C++ library for the Kraken and Binance Spot REST and WebSocket APIs
//
// Copyright (c) 2026 Rob Tomlin
//
// Licensed under the MIT License. See LICENSE file in the project root for
// full license information.
// =============================================================================

#include <benchmark/benchmark.h>

// Canary benchmark proving the harness (CMake wiring, FetchContent'd
// google/benchmark, benchmark_main) works end to end. Kept permanently as a
// cheap sanity check alongside the real Kraken benchmarks added in later
// steps of docs/plans/025-kraken-performance-profiling.md.
namespace {

void BM_Noop(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(state.iterations());
    }
}
BENCHMARK(BM_Noop);

} // namespace
