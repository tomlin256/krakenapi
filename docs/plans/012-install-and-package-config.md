# Plan 012 — Install rules + CMake package config

**Status**: Draft — implementing (Rob directed "do the install work")
**Branch**: `feature/multi-exchange-abstraction`

---

## Goal

Make the library installable so a downstream project can
`find_package(krakenapi)` and `target_link_libraries(app PRIVATE
krakenapi::krakenapi)` (and/or `krakenapi::binanceapi`) against an installed
prefix — not only via in-tree `FetchContent`. This was the one piece punted from
[plan 002](002-step-2b-compat-shim.md); Rob's "punt" was a typo.

**Done when**: `cmake --install` into a prefix lays down the four static libs,
the public headers (component-gated), and a CMake package config; and a **fresh
downstream consumer** that does `find_package(krakenapi REQUIRED)` +
`target_link_libraries(app krakenapi::krakenapi krakenapi::binanceapi)` compiles
and links a small program against that prefix.

## What gets installed

- **Targets** → `EXPORT krakenapiTargets`, namespace `krakenapi::`,
  `ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}`:
  - `exchange_common`, `exchange_http` (always),
  - `krakenapi` (if `KRAKENAPI_BUILD_KRAKEN`), `binanceapi` (if
    `KRAKENAPI_BUILD_BINANCE`).
  The adapters carry no own include dir — they inherit the `INSTALL_INTERFACE`
  one from `exchange_common`/`exchange_http`, which are in the same export set.
- **Headers** (component-gated to match the flags):
  - `include/exchange/common/` always,
  - `include/exchange/kraken/` if KRAKEN, `include/exchange/binance/` if BINANCE,
  - the top-level shim headers (`kraken_*.hpp`, `kraken_compat.hpp`,
    `kraken_ws_client.inl`, `ws_reconnect_session.hpp/.inl`) if COMPAT_SHIM.
- **Package config**: `krakenapiConfig.cmake` (from `cmake/krakenapiConfig.cmake.in`
  via `configure_package_config_file`) + `krakenapiConfigVersion.cmake`
  (`write_basic_package_version_file`, `SameMajorVersion`), and the exported
  `krakenapiTargets.cmake`, all to `${CMAKE_INSTALL_LIBDIR}/cmake/krakenapi`.

## Design decisions

1. **Dependency resolution for installed consumers — `find_dependency` in the
   config** *(recommended)*. The libs PUBLIC-link `OpenSSL::SSL`/`Crypto`,
   `CURL::libcurl`, and `nlohmann_json::nlohmann_json`. The generated config
   `include(CMakeFindDependencyMacro)` + `find_dependency(OpenSSL)` +
   `find_dependency(CURL)` + `find_dependency(nlohmann_json)`, so the exported
   targets' link requirements resolve downstream. OpenSSL/CURL are standard
   find-modules. **`nlohmann_json` is FetchContent'd in our build, so an
   *installed* consumer must have it findable** (vcpkg/conan/apt/brew/`find_package`
   — it ships a `nlohmann_jsonConfig.cmake`). This is the conventional handling
   for a header-only PUBLIC dep; we do **not** vendor a copy. *(Alternative —
   bundle nlohmann's single header into our install — rejected as heavier and
   surprising.)* Documented in the install README section.

2. **Project version `0.1.0`** — `project(KRAKENAPI VERSION 0.1.0)`. The repo had
   none; `write_basic_package_version_file` needs one. Pre-1.0 signals the API is
   still settling (the `kraken::` shim is deprecated, the layout is new). Easy to
   bump later; flag if you'd prefer a different start.

3. **Gate install behind `KRAKENAPI_INSTALL`**, defaulting to **on for a
   top-level build, off as a subproject** (`if(CMAKE_SOURCE_DIR STREQUAL
   CMAKE_CURRENT_SOURCE_DIR)` — 3.15-compatible, avoids polluting a parent
   project's install when krakenapi is `FetchContent`-ed). Tests/examples are
   **not** installed (dev artifacts).

4. **Use `GNUInstallDirs`** for `lib`/`include`/`bin` destinations (portable;
   respects `CMAKE_INSTALL_PREFIX`).

## Step 1 — Install machinery + downstream verification

- `project(KRAKENAPI VERSION 0.1.0)`; `option(KRAKENAPI_INSTALL …)` with the
  top-level default; `include(GNUInstallDirs)`.
- `src/CMakeLists.txt`: add each `add_library` to `install(TARGETS … EXPORT
  krakenapiTargets …)` (gated by the same flags).
- New `cmake/krakenapiConfig.cmake.in` (find_dependency block + include the
  targets file).
- Top-level (guarded by `KRAKENAPI_INSTALL`): the header `install(DIRECTORY …)`
  rules, `install(EXPORT …)`, `configure_package_config_file` +
  `write_basic_package_version_file` + their `install(FILES …)`.
- **Verify**: `cmake --build build` + `ctest` still green (304); then
  `cmake --install build --prefix /tmp/kapi-prefix`; then configure+build a tiny
  throwaway consumer (`find_package(krakenapi REQUIRED)`, a `main` that
  constructs a `BinanceRestClient` and a `KrakenRestClient` and names a
  `kraken::TickPrice`) against `-DCMAKE_PREFIX_PATH=/tmp/kapi-prefix` — must
  configure, compile, link.
- **Checkpoint**: `feat: add install rules + krakenapi CMake package config`.

## Step 2 — Docs + wrap-up

- README: an "Installing" subsection (`cmake --install`, the `find_package` +
  `target_link_libraries` snippet, the `nlohmann_json` note from decision 1).
- CLAUDE.md: note the install/package-config targets and `KRAKENAPI_INSTALL`.
- Plan 002: flip its punt note — install is now done here (link to plan 012).
- Flip plan 012 → Done. Final build + `ctest`.
- **Checkpoint**: `docs: mark plan 012 done — install/package config`.

## Self-review — risks & assumptions

| Risk / assumption | Likelihood | Mitigation |
|---|---|---|
| Installed consumer can't find `nlohmann_json` (we fetch it; they may not have it) | Med | Decision 1 + README note make the dependency explicit; it's a ubiquitous package. The downstream-consumer verification compiles against a prefix where nlohmann *is* available (our build tree's find result) — and the config's `find_dependency` surfaces a clear error otherwise |
| Adapters lack their own `INSTALL_INTERFACE` include dir | None | They inherit it transitively from `exchange_common`/`exchange_http` (same export set) — verified those carry `$<INSTALL_INTERFACE:include>` |
| Component-gated headers miss a file (e.g. a shim `.inl`) → consumer include fails | Low | The downstream consumer test includes through `kraken::` and `exchange::{kraken,binance}::` headers, exercising the installed tree |
| `KRAKENAPI_INSTALL` default misfires under FetchContent | Low | `CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR` is the standard 3.15-safe top-level check |
| Static-lib export with FetchContent deps (OpenSSL/CURL system, nlohmann fetched) mixes find-module + config deps | Low | `find_dependency` handles both; the consumer build is the proof |

**Assumption**: installing the static libs (not shared) is the intent — matches
the current `STATIC` targets; no SONAME/versioned-.so concerns.
