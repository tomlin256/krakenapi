if(CRYPTOCOGS_BUILD_KRAKEN)
  add_executable(kraken_benchmarks bench_smoke.cpp bench_kraken_rest.cpp bench_kraken_ws.cpp bench_tick_price.cpp)
  target_link_libraries(kraken_benchmarks kraken benchmark::benchmark_main)
endif()

if(CRYPTOCOGS_BUILD_BINANCE)
  add_executable(binance_benchmarks bench_binance_ws.cpp)
  target_link_libraries(binance_benchmarks binance benchmark::benchmark_main)
endif()

if(CRYPTOCOGS_BUILD_COINBASE)
  add_executable(coinbase_benchmarks bench_coinbase_ws.cpp)
  target_link_libraries(coinbase_benchmarks coinbase benchmark::benchmark_main)
endif()
