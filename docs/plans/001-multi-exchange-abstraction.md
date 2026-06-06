# 001 — Multi-Exchange Abstraction

**Goal**: Generalise the krakenapi request/response scaffold so that Kraken and Binance (and future exchanges) share a single set of client, connection, and response-envelope types. Only authentication schemes, endpoint paths, and message formats differ per exchange.

---

## Motivation

The current library bakes Kraken-specific names into every structural layer:
- Namespaces: `kraken::`, `kraken::rest::`, `kraken::ws::`
- Headers: `kraken_rest_api.hpp`, `kraken_ws_api.hpp`, etc.
- Client classes: `KrakenRestClient`, `KrakenWsClient`

The scaffold underneath — `TypedPublicRequest<R>`, `RestResponse<T>`, `IWsConnection`, the dispatch loop in `KrakenWsClient`, `SubscriptionHandle` — is fully exchange-agnostic. Extracting it as a shared layer lets a Binance (or any future) adapter reuse the entire machinery and only supply:

1. Per-exchange request/response structs (field mapping + JSON serialisation)
2. A per-exchange `identify_message()` function (JSON frame → dispatch kind)
3. A per-exchange auth adapter (signing + header injection)

---

## Proposed Repository Layout

```
include/
  exchange/
    common/
      rest.hpp            # TypedPublicRequest<R>, TypedPrivateRequest<R>,
                          #   RestResponse<T>, HttpRequest, IRestAuth
      ws.hpp              # IWsConnection, WsResponse<T>, SubscriptionHandle,
                          #   SubscribeResponse, BaseWsResponse, MessageFrame
      ws_client.hpp       # GenericWsClient<MsgId> — exchange-agnostic dispatch
      ws_client.inl       # Template bodies (included by ws_client.hpp)
      types.hpp           # Canonical exchange-agnostic enums:
                          #   Side {Buy, Sell}, OrderType {Limit, Market, …},
                          #   TimeInForce {GTC, GTD, IOC}, OrderStatus
    kraken/
      auth.hpp            # Credentials, sign(), make_nonce()
      types.hpp           # Kraken-specific: PriceType, TriggerReference, StpType,
                          #   FeePreference, TickPrice, OrderParams, Triggers,
                          #   Conditional, OrderDescription, OrderInfo, etc.
      rest_api.hpp        # All Kraken REST request/response structs
      ws_api.hpp          # All Kraken WS request/response structs
      rest_client.hpp     # KrakenRestClient (wraps exchange::rest::GenericRestClient
                          #   with KrakenAuth)
      ix_ws_connection.hpp# IxWsConnection + URL-string make_ws_client overload
      reconnect_session.hpp
    binance/
      auth.hpp            # BinanceCredentials {api_key, secret_key, Algorithm}
                          #   Algorithm: HmacSha256 | Rsa | Ed25519
      types.hpp           # Binance-specific types and enums
      rest_api.hpp        # Binance REST request/response structs
      ws_api.hpp          # Binance WebSocket API request/response structs
      ws_streams.hpp      # Binance market/user data stream types (separate channel
                          #   architecture: listen key, named streams)
      rest_client.hpp     # BinanceRestClient
      ws_client.hpp       # BinanceWsClient + BinanceStreamClient
src/
  kraken_rest_client.cpp  # Renamed/moved; remains in src/
  kraken_ws_client.cpp
  kraken_types.cpp
  binance_rest_client.cpp # New
  binance_ws_client.cpp   # New
tests/
  unit/
    (existing Kraken tests, updated for new namespaces/headers)
    test_binance_auth.cpp
    test_binance_rest_requests.cpp
    test_binance_rest_responses.cpp
    test_binance_ws_client.cpp
  examples/
    (existing examples, header paths updated)
    binance_public_rest.cpp
    binance_ws_streams.cpp
```

### Namespace layout after migration

| Namespace | Contains |
|---|---|
| `exchange::` | Canonical enums (Side, OrderType, TimeInForce, OrderStatus) |
| `exchange::rest::` | TypedPublicRequest<R>, TypedPrivateRequest<R>, RestResponse<T>, HttpRequest, IRestAuth |
| `exchange::ws::` | IWsConnection, WsResponse<T>, SubscriptionHandle, GenericWsClient<MsgId>, BaseWsResponse |
| `exchange::kraken::` | Kraken-specific enums, TickPrice, OrderParams, Triggers, Conditional, OrderInfo, etc. |
| `exchange::kraken::rest::` | All Kraken REST req/resp types, Credentials, KrakenRestClient |
| `exchange::kraken::ws::` | All Kraken WS req/resp types, SubscribeChannel, IxWsConnection, WsCredentials |
| `exchange::binance::` | Binance-specific types |
| `exchange::binance::rest::` | All Binance REST req/resp types, BinanceCredentials, BinanceRestClient |
| `exchange::binance::ws::` | Binance WS API types, BinanceWsClient |
| `exchange::binance::streams::` | Market/user stream types (listen key sessions, named stream channels) |

**Backwards-compatibility aliases** (`include/kraken_*.hpp` forwarding headers) are intentionally **not** provided — callers update their include paths once as part of adopting this library version.

---

## Key Architectural Decisions

### A. `GenericWsClient<MsgId>` — parameterised dispatch

The entire `KrakenWsClient` dispatch loop (pending-handler map, subscription-callback map, pre-connection queue, `SubscriptionHandle`, thread safety) is exchange-agnostic. Only `identify_message(const json&)` differs. The refactored client is:

```cpp
// exchange/common/ws_client.hpp
namespace exchange::ws {

enum class FrameKind { MethodResponse, PushMessage, Unknown };

struct FrameDescriptor {
    FrameKind kind;
    std::optional<int64_t> req_id;   // set when kind == MethodResponse
    std::string channel;             // set when kind == PushMessage
};

using MessageIdentifier = std::function<FrameDescriptor(const json&)>;

class GenericWsClient : public std::enable_shared_from_this<GenericWsClient> {
public:
    explicit GenericWsClient(std::shared_ptr<IWsConnection> conn,
                             MessageIdentifier identifier);
    void init();
    // execute / execute_async / subscribe / subscribe_async — identical to current
    // KrakenWsClient signatures, but templated on request types that satisfy:
    //   Req::response_type, Req::to_json(), Response::from_json(json)
    // (No exchange-specific types in this class)
};

} // namespace exchange::ws
```

Each exchange provides its own `identify_message` function and wraps `GenericWsClient` in a thin type alias or factory:

```cpp
// exchange/kraken/ws_api.hpp
namespace exchange::kraken::ws {
    exchange::ws::FrameDescriptor identify_message(const json& j);
}

// exchange/kraken/ix_ws_connection.hpp
inline std::shared_ptr<exchange::ws::GenericWsClient>
make_kraken_ws_client(const std::string& url) {
    auto conn = std::make_shared<IxWsConnection>(url);
    return exchange::ws::make_generic_ws_client(conn,
               exchange::kraken::ws::identify_message);
}
```

### B. `IRestAuth` — authentication strategy

```cpp
// exchange/common/rest.hpp
namespace exchange::rest {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct IRestAuth {
    virtual ~IRestAuth() = default;
    // Called once per request; injects nonce, timestamp, signature into req.
    virtual void sign(HttpRequest& req) const = 0;
};

} // namespace exchange::rest
```

`KrakenAuth` implements `IRestAuth` with the existing HMAC-SHA512 scheme.
`BinanceAuth` implements `IRestAuth` with HMAC-SHA256 (or RSA/Ed25519), injecting `X-MBX-APIKEY`, `timestamp`, `recvWindow`, and `signature`.

### C. `RestResponse<T>` — common error envelope

Both exchanges use different on-wire error formats but callers see the same type:

```cpp
// exchange/common/rest.hpp
template<typename T>
struct RestResponse {
    bool ok{false};
    std::optional<T> result;
    std::vector<std::string> errors;
};
```

Each exchange's client parses its own wire format (Kraken: `{"error":[],"result":{}}`, Binance: `{"code":-1121,"msg":"..."}`) and normalises into this envelope. The Kraken parse helper `parse_rest_response<T>()` moves to `exchange::kraken::rest::`.

### D. Binance WebSocket — two channels, one client

Binance has two distinct WebSocket protocols:

1. **WebSocket API** (`wss://ws-api.binance.com/ws-api/v3`) — bidirectional trading (same req/response pattern as Kraken WS). Uses `GenericWsClient`.
2. **WebSocket Streams** (`wss://stream.binance.com/stream`) — market data / user data push only. Subscribes by URL path or `SUBSCRIBE` method. Also uses `GenericWsClient` but with a Binance-stream-specific `identify_message`.

Both are instances of `GenericWsClient` with different identifiers and connection URLs.

---

## Steps

### Step 1 — Create common scaffold headers (no logic change)

**Done when**: New headers exist; existing code compiles after `#include` path update.

- Create `include/exchange/common/rest.hpp` — copy `TypedPublicRequest<R>`, `TypedPrivateRequest<R>`, `RestResponse<T>`, `HttpRequest`, `IRestAuth` (stub) from `kraken_rest_api.hpp`/`kraken_rest_client.hpp`. Namespace: `exchange::rest::`.
- Create `include/exchange/common/ws.hpp` — copy `IWsConnection`, `WsResponse<T>`, `SubscriptionHandle`, `BaseWsResponse` from `kraken_ws_client.hpp`/`kraken_ws_api.hpp`. Namespace: `exchange::ws::`.
- Create `include/exchange/common/ws_client.hpp` + `.inl` — `GenericWsClient` taking `MessageIdentifier`. Namespace: `exchange::ws::`.
- Create `include/exchange/common/types.hpp` — `Side`, `OrderType`, `TimeInForce`, `OrderStatus` (extracted from `kraken_types.hpp`). Namespace: `exchange::`.
- No files deleted yet; Kraken headers `#include` the new common headers internally and re-export via `using`.
- **Tests**: Full build + all existing tests pass. No behaviour change.

### Step 2 — Migrate Kraken to `exchange/kraken/` namespaces

**Done when**: All Kraken code lives under `exchange::kraken::*`; all tests pass.

- Create `include/exchange/kraken/auth.hpp` — move `Credentials`, `sign()`, `make_nonce()`, crypto helpers. Namespace: `exchange::kraken::rest::`.
- Create `include/exchange/kraken/types.hpp` — Kraken-specific types (PriceType, TriggerReference, StpType, FeePreference, TickPrice, OrderParams, Triggers, Conditional, OrderDescription, OrderInfo, TradeInfo, LedgerEntry). Namespace: `exchange::kraken::`.
- Create `include/exchange/kraken/rest_api.hpp` — all REST req/resp structs, now using `exchange::kraken::rest::` namespace.
- Create `include/exchange/kraken/ws_api.hpp` — all WS req/resp structs, `exchange::kraken::ws::`.
- Create `include/exchange/kraken/rest_client.hpp` — `KrakenRestClient` wraps `GenericRestClient` + `KrakenAuth : IRestAuth`.
- Create `include/exchange/kraken/ix_ws_connection.hpp` — `IxWsConnection` + `make_kraken_ws_client()`.
- Create `include/exchange/kraken/reconnect_session.hpp` — `WsReconnectSession`.
- Update `src/kraken_*.cpp` to use new headers/namespaces.
- Delete old `include/kraken_*.hpp` files once all includes updated.
- Update `tests/unit/` includes and namespace references.
- **Tests**: Full build + all unit tests pass. Examples compile (update includes).

### Step 3 — Add Binance authentication and REST client

**Done when**: `BinanceRestClient` executes public requests; auth signs private requests correctly; unit tests verify signatures.

- Create `include/exchange/binance/auth.hpp`:
  - `enum class BinanceSignAlgorithm { HmacSha256, Rsa, Ed25519 }`.
  - `struct BinanceCredentials { api_key, secret_key, BinanceSignAlgorithm }`.
  - `BinanceAuth : IRestAuth` — injects `X-MBX-APIKEY` header, appends `timestamp` + `recvWindow` + `signature` to query string.
  - HMAC-SHA256 signing in Step 3; RSA/Ed25519 deferred to a later step.
- Create `include/exchange/binance/rest_client.hpp` + `src/binance_rest_client.cpp` — `BinanceRestClient`, mirrors `KrakenRestClient` interface. Default base URL: `https://api.binance.com`.
- Create `tests/unit/test_binance_auth.cpp` — verify HMAC-SHA256 signature against Binance's documented test vector.
- **Tests**: New tests pass; all Kraken tests still pass.

### Step 4 — Binance REST public endpoints

**Done when**: Market data endpoints are implemented and unit-tested with mock HTTP performers.

Endpoints to implement (in `include/exchange/binance/rest_api.hpp`):

| Request type | Method + Path | Response type |
|---|---|---|
| `BinancePingRequest` | GET `/api/v3/ping` | `BinancePing` |
| `BinanceServerTimeRequest` | GET `/api/v3/time` | `BinanceServerTime` |
| `BinanceExchangeInfoRequest` | GET `/api/v3/exchangeInfo` | `BinanceExchangeInfo` |
| `BinanceTickerPriceRequest` | GET `/api/v3/ticker/price` | `BinanceTickerPrice` |
| `BinanceTicker24hrRequest` | GET `/api/v3/ticker/24hr` | `BinanceTicker24hr` |
| `BinanceKlinesRequest` | GET `/api/v3/klines` | `BinanceKlinesResult` |
| `BinanceOrderBookRequest` | GET `/api/v3/depth` | `BinanceOrderBook` |
| `BinanceRecentTradesRequest` | GET `/api/v3/trades` | `BinanceTradesResult` |

- Each inherits `exchange::rest::TypedPublicRequest<R>`.
- `from_json()` deserialises Binance's JSON format (arrays for klines/trades, objects for ticker).
- Create `tests/unit/test_binance_rest_requests.cpp` and `test_binance_rest_responses.cpp`.
- Add `examples/binance_public_rest.cpp`.
- **Tests**: All unit tests pass; example compiles and runs against live Binance (no credentials needed).

### Step 5 — Binance REST private (account + trading) endpoints

**Done when**: Account and order endpoints implemented and unit-tested.

Endpoints to implement:

| Request type | Method + Path | Response type |
|---|---|---|
| `BinanceAccountRequest` | GET `/api/v3/account` | `BinanceAccount` |
| `BinanceOpenOrdersRequest` | GET `/api/v3/openOrders` | `BinanceOpenOrdersResult` |
| `BinanceAllOrdersRequest` | GET `/api/v3/allOrders` | `BinanceAllOrdersResult` |
| `BinanceNewOrderRequest` | POST `/api/v3/order` | `BinanceNewOrderResponse` |
| `BinanceCancelOrderRequest` | DELETE `/api/v3/order` | `BinanceCancelOrderResponse` |
| `BinanceCancelAllOpenOrdersRequest` | DELETE `/api/v3/openOrders` | `BinanceCancelAllResponse` |
| `BinanceMyTradesRequest` | GET `/api/v3/myTrades` | `BinanceMyTradesResult` |

- All inherit `exchange::rest::TypedPrivateRequest<R>`.
- `BinanceNewOrderRequest` maps `exchange::Side` and `exchange::OrderType` to Binance wire strings (`"BUY"/"SELL"`, `"LIMIT"/"MARKET"`).
- Unit tests use `make_test_client()` (same pattern as Kraken tests).
- **Tests**: All unit tests pass.

### Step 6 — Binance WebSocket market streams

**Done when**: `BinanceStreamClient` subscribes to ticker and trade streams; unit-tested with `MockWsConnection`.

- Create `include/exchange/binance/ws_streams.hpp`:
  - `BinanceStreamIdentifier` — `identify_message()` for stream frames (inspects `"stream"`, `"data"` fields).
  - Push message types: `BinanceAggTradeEvent`, `BinanceTickerEvent`, `BinanceMiniTickerEvent`, `BinanceKlineEvent`, `BinanceDepthUpdateEvent`.
  - `BinanceSubscribeRequest` / `BinanceUnsubscribeRequest` (params: `["<symbol>@<stream>", ...]`).
- `make_binance_stream_client(url)` factory — returns `GenericWsClient` configured with `BinanceStreamIdentifier`.
- Create `tests/unit/test_binance_ws_client.cpp` — mock-based tests for subscribe lifecycle.
- Add `examples/binance_ws_streams.cpp`.
- **Tests**: All unit tests pass.

### Step 7 — Binance WebSocket API (bidirectional trading)

**Done when**: `BinanceWsClient` can place and cancel orders over WebSocket.

- Create `include/exchange/binance/ws_api.hpp`:
  - `BinanceWsCredentials` (session-based: either logon flow or `X-MBX-APIKEY` per request).
  - Request/response pairs: `BinanceWsNewOrderRequest → BinanceWsNewOrderResponse`, `BinanceWsCancelOrderRequest → BinanceWsCancelOrderResponse`, `BinanceWsPingRequest → BinanceWsPongMessage`.
  - `BinanceWsIdentifier` — `identify_message()` for WS API frames (Binance uses `"id"` + `"status"` fields).
- `make_binance_ws_api_client(url)` factory.
- Add unit tests.
- **Tests**: All unit tests pass.

### Step 8 — CMake, build validation, final cleanup

**Done when**: Full build from clean with all tests passing; examples compile; CI-equivalent local check.

- Update `src/CMakeLists.txt`:
  - `krakenapi::krakenapi` target — Kraken sources only (existing).
  - New `krakenapi::binanceapi` target — Binance sources + links `krakenapi::krakenapi` for common scaffold.
  - Common scaffold (`include/exchange/common/`) is header-only; no separate lib.
- Update top-level `CMakeLists.txt` to `add_subdirectory` any new source directories.
- Update `tests/CMakeLists.txt` to add Binance unit test executables.
- Remove any dead code or stale comments from the refactor.
- Run `ctest --output-on-failure` — all tests pass.
- Verify examples compile against the restructured headers.
- Update `CLAUDE.md` to reflect new namespace layout, file structure, and patterns.

---

## Self-Review — Risks, Assumptions, and Open Questions

### Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| Breaking existing consumers of `kraken_*.hpp` headers | High (intentional) | This is a deliberate breaking change; callers update once. No legacy forwarding headers. |
| `GenericWsClient` template complexity makes errors harder to read | Medium | Keep `MessageIdentifier` as a runtime `std::function`, not a template param — avoids template error cascades. |
| Binance HMAC-SHA256 signing: query + body concatenation differs subtly from Kraken | Medium | Step 3 includes a known-good test vector from Binance docs. |
| Binance RSA/Ed25519 signing omitted from plan | Low (intentional) | HMAC-SHA256 covers the common case; RSA/Ed25519 can be added later with the same `IRestAuth` extension point. |
| Binance WebSocket authentication: logon-flow vs. per-request signing | Medium | Step 7 implements per-request API-key approach first; logon session deferred. |
| `GenericWsClient` shares dispatch for both Kraken and Binance — bugs are shared | Low | The dispatch logic is the same for both; exchange-specific bugs will be in `identify_message()`, which is isolated. |
| Binance timestamp (`ms` vs `μs`) requires careful serialisation | Low | Wrap in `BinanceAuth::sign()` using milliseconds by default; add `microseconds` flag later. |

### Assumptions

1. The canonical `exchange::OrderType` enum covers all values both exchanges share (Limit, Market). Kraken-only types (Iceberg, TrailingStopLimit, SettlePosition) stay in `exchange::kraken::`. Binance-only types (e.g. STOP\_LOSS\_LIMIT, TAKE\_PROFIT\_LIMIT, TRAILING\_STOP\_MARKET) stay in `exchange::binance::`.
2. Binance `recvWindow` defaults to 5000 ms and is not exposed on every request struct — callers can set it on `BinanceCredentials`.
3. Binance user data streams (listen key mechanism) are out of scope for this plan; only named market streams and the WS API are covered.
4. Example programs link against ixwebsocket for real connections; unit tests remain fully mock-based.
5. Two static libraries are produced: `libkrakenapi.a` (Kraken-only; existing CMake target `krakenapi::krakenapi`) and `libbinanceapi.a` (new target `krakenapi::binanceapi`). Common scaffold headers are header-only and shared by both. The repo name stays `krakenapi` for now.
6. Example programs link against ixwebsocket for real connections; unit tests remain fully mock-based.

### Deferred items

**RSA and Ed25519 signing for Binance** — Binance supports three signature algorithms. HMAC-SHA256 is the most common and is implemented in Step 3. RSA (PKCS#8 private key, SHA-256 digest) and Ed25519 are alternatives that Binance recommends for higher-security or high-throughput use cases; Ed25519 is the fastest. Both are supported by the `IRestAuth` extension point already in the design — adding them later means implementing a new `BinanceAuth` subclass and wiring `BinanceSignAlgorithm::Rsa` / `::Ed25519` in `BinanceCredentials`. No structural changes required.

**Binance user data streams (listen key mechanism)** — Binance supports a real-time feed of account events (order fills, balance changes, position updates) delivered over WebSocket. Unlike market streams, access requires first calling a REST endpoint (`POST /api/v3/userDataStream`) to obtain a short-lived *listen key*, then connecting to a stream URL of the form `wss://stream.binance.com/ws/<listenKey>`. The key must be kept alive via periodic `PUT` pings (every 30 minutes) and can be closed with `DELETE`. This is structurally distinct from the WS API (bidirectional trading) and named market streams, and requires a small session-management wrapper around the listen key lifecycle. Deferred to a follow-on plan; the `IWsConnection` and `GenericWsClient` plumbing already supports it.
