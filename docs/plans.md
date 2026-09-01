# Implementation Plans

| # | Plan | Status |
|---|---|---|
| 001 | [Multi-exchange abstraction — common scaffold + Binance adapter](plans/001-multi-exchange-abstraction.md) | Draft |
| 001 | ↳ [Appendix: Binance message formats](plans/001-appendix-binance-message-formats.md) | Reference |
| 001 | ↳ [Appendix: Testing strategy](plans/001-appendix-testing-strategy.md) | Reference |
| 001 | ↳ [Appendix: Migration guide for existing cryptocogs users](plans/001-appendix-migration-guide.md) | Reference |
| 001 | ↳ [Appendix: Shipped backwards-compatibility shim](plans/001-appendix-compat-shim.md) | Reference |
| 002 | [Step 2b — Ship the Kraken backwards-compatibility shim](plans/002-step-2b-compat-shim.md) | Done · removed in 014 |
| 003 | [Step 5 — Binance REST public (market data) endpoints](plans/003-step-5-binance-rest-public.md) | Done |
| 004 | [Step 6 — Binance REST private (account + trading) endpoints](plans/004-step-6-binance-rest-private.md) | Done |
| 005 | [Step 7 — Binance WebSocket market streams](plans/005-step-7-binance-ws-streams.md) | Done |
| 006 | [Step 8 — Binance WebSocket API (bidirectional trading)](plans/006-step-8-binance-ws-api.md) | Done |
| 007 | [Step 9 — CMake target split, build toggles, full-matrix validation](plans/007-step-9-cmake-build-validation.md) | Done |
| 008 | [Step 10 — Agent onboarding guide for new exchanges](plans/008-step-10-agent-onboarding-guide.md) | Done |
| 009 | [Relocate `TickPrice` from the Kraken adapter to the common layer](plans/009-relocate-tickprice-to-common.md) | Done |
| 010 | [Extract a shared cURL HTTP transport (de-duplicate REST clients)](plans/010-shared-curl-http-transport.md) | Done |
| 011 | [Fully generic `RestClient` (unify the typed execute path)](plans/011-generic-rest-client.md) | Proposed (deferred) |
| 012 | [Install rules + CMake package config](plans/012-install-and-package-config.md) | Done |
| 013 | [Source-compat shim for legacy `src/kraken_{ws,rest}_client.cpp` paths](plans/013-source-compat-shim.md) | Done · removed in 014 |
| 014 | [Remove the deprecated `kraken_*.hpp` / `kraken::` compatibility shim (header + source)](plans/014-remove-compat-shim.md) | Done |
| 015 | [Rename the project `krakenapi` → `cryptocogs`](plans/015-rename-to-cryptocogs.md) | Done |
| 016 | [Make all headers purely declarative (project-wide hpp/inl/cpp split)](plans/016-declarative-headers.md) | Done |
| 017 | [Fix two CMake install nits (ixwebsocket install + cxx_std_17 export)](plans/017-cmake-install-nits.md) | Done |
| 018 | [Coinbase Exchange adapter — REST + WebSocket](plans/018-coinbase-exchange-adapter.md) | Done |
| 019 | [Coinbase FIX order entry](plans/019-coinbase-fix-order-entry.md) | Proposed (deferred) |
| 020 | [Crypto.com Exchange adapter — REST + WebSocket](plans/020-cryptocom-exchange-adapter.md) | Done |
| 021 | [`from_file` credential loaders for Binance, Coinbase, Crypto.com](plans/021-credentials-from-file.md) | Done |
| 022 | [Rename Kraken `Credentials` → `KrakenCredentials`](plans/022-rename-kraken-credentials.md) | Done |
| 023 | [Crypto.com `user.trade` WS channel](plans/023-cryptocom-user-trade-channel.md) | Stub |
| 024 | [`exec_inst` (post-only) support on `CryptoComCreateOrderRequest`](plans/024-cryptocom-create-order-exec-inst.md) | Done |
| 025 | [Kraken performance profiling — benchmark harness + hot-spot findings](plans/025-kraken-performance-profiling.md) | Done |
| 026 | [Pre-size `std::vector` growth in Kraken `from_json` (`.reserve()`)](plans/026-kraken-reserve-vector-growth.md) | Done |
| 027 | [Benchmark + `.reserve()` vector growth for Binance/Coinbase/Crypto.com](plans/027-remaining-adapters-reserve-vector-growth.md) | In progress |
