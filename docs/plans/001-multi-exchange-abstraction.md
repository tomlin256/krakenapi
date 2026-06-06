# 001 — Multi-Exchange Abstraction

**Goal**: Generalise the krakenapi request/response scaffold so that Kraken and Binance (and future exchanges) share a single set of client, connection, and response-envelope types. Only authentication schemes, endpoint paths, and message formats differ per exchange.

## Companion documents

This file is the architecture overview and step plan. Two companion docs hold the detail that the steps depend on:

- **[001-appendix-binance-message-formats.md](001-appendix-binance-message-formats.md)** — every Binance REST and WebSocket message captured verbatim from the live API docs, annotated with field types and the dispatch routing key. This is the source material for the test fixtures (the Binance analog of `tests/unit/ws_client_example_json.hpp`).
- **[001-appendix-testing-strategy.md](001-appendix-testing-strategy.md)** — the test approach mirrored from the existing Kraken suite: captured-frame fixture headers, `from_json` parse tests asserting every field, request-build tests, `identify_message` tests, and mock-performer / `MockWsConnection` end-to-end tests. Lists every new Binance test file and what it covers.

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
    binance_rest_example_json.hpp      # captured REST fixtures (cf. ws_client_example_json.hpp)
    binance_account_example_json.hpp   # captured account/order fixtures
    binance_ws_stream_example_json.hpp # captured WS stream push frames
    binance_ws_api_example_json.hpp    # captured WS API replies
    test_binance_auth.cpp
    test_binance_rest_requests.cpp
    test_binance_rest_responses.cpp
    test_binance_ws_client.cpp
  examples/
    (existing examples, header paths updated)
    binance_public_rest.cpp
    binance_ws_streams.cpp
```

> Fixture headers and the full test matrix are detailed in [001-appendix-testing-strategy.md](001-appendix-testing-strategy.md); their captured JSON comes from [001-appendix-binance-message-formats.md](001-appendix-binance-message-formats.md).

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

### A. `GenericWsClient` — runtime-parameterised dispatch

The entire `KrakenWsClient` dispatch loop (pending-handler map, subscription-callback map, pre-connection queue, `SubscriptionHandle`, thread safety) is exchange-agnostic. Only `identify_message(const json&)` differs. The refactored client is:

```cpp
// exchange/common/ws_client.hpp
namespace exchange::ws {

enum class FrameKind { MethodResponse, PushMessage, Unknown };

struct FrameDescriptor {
    FrameKind kind;
    // MethodResponse / subscribe-ack: correlates the reply to its request.
    std::optional<std::string> correlation_id;
    // PushMessage: the subscription routing key the callback was registered under.
    std::string route_key;
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

**Why `correlation_id` is a `std::string`, not `int64_t`** — Kraken correlates replies by an integer `req_id`; Binance's WebSocket API correlates by an `id` that may be an integer *or* a string (their docs show UUID strings). Keying the internal `pending_` map on `std::string` covers both: the Kraken adapter stringifies its generated `int64_t` req_id before sending and the wire format still emits it as a JSON integer; the Binance adapter uses its `id` directly. The correlation key is internal-only — it never dictates the wire type.

**Why `route_key` instead of `channel`** — Kraken routes push frames by a `"channel"` field (`"ticker"`, `"book"`). Binance combined streams route by a `"stream"` field (`"btcusdt@aggTrade"`), and raw single-stream connections carry only an event-type `"e"` field. `route_key` is whatever string the subscribe request registered the callback under; each exchange's `identify_message` extracts the matching key from an inbound frame. The `subscriptions_` map and `SubscriptionHandle` are otherwise unchanged.

The concrete `correlation_id` / `route_key` derivation for every Binance frame is tabulated in [001-appendix-binance-message-formats.md](001-appendix-binance-message-formats.md).

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

Each exchange's client parses its own wire format and normalises into this envelope:

| | Kraken | Binance |
|---|---|---|
| Success shape | `{"error":[],"result":{…}}` | bare object/array (`{…}` or `[…]`) — no envelope |
| Error shape | `{"error":["EOrder:…"],"result":{}}` | `{"code":-1121,"msg":"Invalid symbol."}` |
| Success signal | `error` array empty | HTTP 2xx **and** no `code`/`msg` error object |
| `ok` derivation | `error.empty()` | `http_status < 400 && !has("code")` |

Two consequences for the Binance client:
1. **No wrapping envelope** — the result payload *is* the top-level JSON, so `parse_binance_response<T>()` passes the whole body to `T::from_json` on success and reads `code`/`msg` on failure. A success body can be a JSON array (e.g. `myTrades`, `openOrders`), which the helper handles.
2. **HTTP status matters** — unlike Kraken (always 200 + error array), Binance signals failure with non-2xx status codes. The HTTP performer must surface the status code to the parser; `HttpRequest`/the performer signature gains a status-code return path.

The Kraken parse helper `parse_rest_response<T>()` moves to `exchange::kraken::rest::`; the Binance helper `parse_binance_response<T>()` lives in `exchange::binance::rest::`. Both yield the same `exchange::rest::RestResponse<T>`.

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
  - `struct BinanceCredentials { api_key, secret_key, BinanceSignAlgorithm, recv_window_ms=5000 }`.
  - `BinanceAuth : IRestAuth` — injects `X-MBX-APIKEY` header, appends `timestamp` (+ `recvWindow`) to the query/body, computes the signature over the **entire** `query_string + body` concatenation, and appends `&signature=<sig>`.
  - HMAC-SHA256 signing in Step 3; RSA/Ed25519 deferred (see Deferred items).
- **Signing differences from Kraken** (these are the easy-to-get-wrong bits — call them out in the test):
  | | Kraken | Binance HMAC |
  |---|---|---|
  | Digest | HMAC-**SHA512** | HMAC-**SHA256** |
  | Signed payload | `path + SHA256(nonce + body)` | `query_string + body` (concatenated, already URL-encoded) |
  | Output encoding | **base64** | **lowercase hex** |
  | Anti-replay | `nonce` (µs counter) | `timestamp` (ms) + optional `recvWindow` |
  | Key material | base64-decoded secret | raw UTF-8 secret bytes |
  - Reuse the existing `hmac_*` OpenSSL helpers; add `hmac_sha256()` + a `to_hex()` encoder alongside the existing `base64_encode()`.
- Create `include/exchange/binance/rest_client.hpp` + `src/binance_rest_client.cpp` — `BinanceRestClient`, mirrors `KrakenRestClient` interface. Default base URL: `https://api.binance.com`. The HTTP performer must return the response **status code** as well as the body (see envelope section C).
- Create `tests/unit/test_binance_auth.cpp` — verify HMAC-SHA256 hex signature against Binance's published worked example (the SPOT `order` example in *REST API → SIGNED endpoint examples*, which gives a known key/secret/params → expected signature). This is the Binance analog of `test_signature.cpp`.
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
- **Format specifics** (full JSON in the appendix; these drive the field types):
  - Numbers arrive as **JSON strings** (`"4.00000200"`), same as Kraken — parse with `std::stod(j.value("field","0"))`.
  - Timestamps are **integer milliseconds** (`1499865549590`), *not* ISO-8601 strings — store as `int64_t`. (Kraken WS uses ISO strings; do not copy that here.)
  - `klines` is an **array of 12-element mixed arrays** — parse positionally by index, not by key.
  - `depth` bids/asks are **2-element string arrays** `["price","qty"]` — parse positionally.
  - `ticker/price`, `ticker/24hr`, `bookTicker` accept a single `symbol` or a `symbols=[...]` list; with a list the response becomes a JSON **array** of the object. Support both (single object vs array) in `from_json`.
- Create `tests/unit/binance_rest_example_json.hpp` — captured response fixtures (the REST analog of `ws_client_example_json.hpp`; see testing-strategy doc). Populate from the appendix.
- Create `tests/unit/test_binance_rest_requests.cpp` (path/method/query) and `test_binance_rest_responses.cpp` (`from_json` field assertions against the fixtures).
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
- `BinanceNewOrderRequest` maps `exchange::Side` → `"BUY"/"SELL"` and `exchange::OrderType` → `"LIMIT"/"MARKET"/…` (uppercase wire strings; Binance-only types like `STOP_LOSS_LIMIT` live in `exchange::binance::`).
- **Format specifics**:
  - `POST /api/v3/order` has three response shapes selected by the `newOrderRespType` param: **ACK** (ids only), **RESULT** (+ fill status), **FULL** (+ `fills[]` array). `BinanceNewOrderResponse` carries all fields as `std::optional` and a `fills` vector that is empty unless FULL. Default for LIMIT/MARKET is FULL.
  - `DELETE /api/v3/openOrders` returns a **JSON array** of cancelled-order objects; `myTrades`, `openOrders`, `allOrders` likewise return arrays.
  - Order fields reuse the string-number + int-ms-timestamp conventions from Step 4.
- Create `tests/unit/binance_account_example_json.hpp` fixtures (account, order ACK/RESULT/FULL, cancel, openOrders, myTrades) — captured from the appendix.
- Unit tests use `make_test_client()` (same injected-performer pattern as `test_client.cpp`): assert the signed request path/query and that `from_json` parses each fixture. Signing correctness is already covered by `test_binance_auth.cpp`.
- **Tests**: All unit tests pass.

### Step 6 — Binance WebSocket market streams

**Done when**: `BinanceStreamClient` subscribes to ticker and trade streams; unit-tested with `MockWsConnection`.

- Create `include/exchange/binance/ws_streams.hpp`:
  - `BinanceStreamIdentifier` — `identify_message()` for stream frames. Use the **combined-stream** endpoint (`wss://stream.binance.com/stream`) so multiple streams share one connection; every push frame is then wrapped as `{"stream":"btcusdt@aggTrade","data":{…}}`. The `route_key` is the `"stream"` value; the subscribe-ack `{"result":null,"id":N}` is a `MethodResponse` with `correlation_id = id`.
  - Push message types: `BinanceAggTradeEvent`, `BinanceTradeEvent`, `BinanceTickerEvent` (`24hrTicker`), `BinanceMiniTickerEvent`, `BinanceKlineEvent`, `BinanceBookTickerEvent`, `BinanceDepthUpdateEvent` (diff) + `BinancePartialDepth` (snapshot).
  - **Format specifics**: push payloads use **terse single-letter keys** (`e`=event, `E`=event-time-ms, `s`=symbol, `p`=price, `q`=qty, `k`=kline-object, …) — `from_json` maps these explicitly. Numbers are strings; times are int-ms. Kline data is nested under `k`.
  - `BinanceSubscribeRequest` / `BinanceUnsubscribeRequest` — `{"method":"SUBSCRIBE","params":["<symbol>@<stream>",…],"id":N}`. Note the subscribe ack carries **no channel/stream echo**, so `SubscriptionHandle` must remember the `route_key` from the request (Kraken's ack echoes the channel; Binance's does not — the generic client already supports this since the handle stores its own key).
- `make_binance_stream_client(url)` factory — returns `GenericWsClient` configured with `BinanceStreamIdentifier`.
- Create `tests/unit/binance_ws_stream_example_json.hpp` — captured push frames + subscribe ack (the direct analog of `ws_client_example_json.hpp`).
- Create `tests/unit/test_binance_ws_client.cpp` — `identify_message` tests (one per event type), `from_json` field assertions, and `MockWsConnection` subscribe-lifecycle tests (fire_open → subscribe ack by id → inject push frame → callback fires → cancel).
- Add `examples/binance_ws_streams.cpp`.
- **Tests**: All unit tests pass.

### Step 7 — Binance WebSocket API (bidirectional trading)

**Done when**: `BinanceWsClient` can place and cancel orders over WebSocket.

- Create `include/exchange/binance/ws_api.hpp`:
  - `BinanceWsCredentials` — per-request signing: `apiKey` + `signature` + `timestamp` inside `params` (the logon-session flow is deferred). Reuses `BinanceAuth`'s HMAC-SHA256 over the sorted `params`.
  - Request/response pairs: `BinanceWsNewOrderRequest → BinanceWsNewOrderResponse` (`method:"order.place"`), `BinanceWsCancelOrderRequest → BinanceWsCancelOrderResponse` (`method:"order.cancel"`), `BinanceWsPingRequest → BinanceWsPongMessage` (`method:"ping"`).
  - **Format specifics**: request is `{"id":"<uuid|int>","method":"…","params":{…}}`; success reply `{"id":…,"status":200,"result":{…},"rateLimits":[…]}`; error reply `{"id":…,"status":400,"error":{"code":-2010,"msg":"…"}}`. `BinanceWsIdentifier` classifies every reply as a `MethodResponse` with `correlation_id = stringify(id)` — there is no `channel`/push concept on the WS API endpoint. `ok` is derived from `status < 400`; on error, populate `WsResponse::error` from `error.msg`.
  - The `id` is generated as an `int64_t` and stringified (the generic client's `correlation_id` is already a string), matching the Kraken adapter's approach.
- `make_binance_ws_api_client(url)` factory (endpoint `wss://ws-api.binance.com/ws-api/v3`).
- Create `tests/unit/binance_ws_api_example_json.hpp` fixtures (success + error replies) and add `identify_message` + `from_json` + `MockWsConnection` execute-lifecycle tests to `test_binance_ws_client.cpp`.
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
