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
      ix_ws_connection.hpp# IxWsConnection — generic ixwebsocket IWsConnection impl
                          #   + make_generic_ws_client(url, identifier) factory
      reconnect_session.hpp # WsReconnectSession — generic reconnect/backoff machinery
      types.hpp           # Canonical exchange-agnostic enums:
                          #   Side {Buy, Sell}, OrderType {Limit, Market, …},
                          #   TimeInForce {GTC, GTD, IOC}, OrderStatus
    kraken/
      auth.hpp            # Credentials, sign(), make_nonce()
      types.hpp           # Kraken-specific: PriceType, TriggerReference, StpType,
                          #   FeePreference, TickPrice, OrderParams, Triggers,
                          #   Conditional, OrderDescription, OrderInfo, etc.
      rest_api.hpp        # All Kraken REST request/response structs
      ws_api.hpp          # All Kraken WS request/response structs + identify_message
      rest_client.hpp     # KrakenRestClient (wraps exchange::rest::GenericRestClient
                          #   with KrakenAuth)
      ws_client.hpp       # PUBLIC_WS_URL/PRIVATE_WS_URL consts + make_kraken_ws_client()
                          #   (thin wrapper over common make_generic_ws_client)
    binance/
      auth.hpp            # BinanceCredentials {api_key, secret_key, Algorithm}
                          #   Algorithm: HmacSha256 | Rsa | Ed25519
      types.hpp           # Binance-specific types and enums
      rest_api.hpp        # Binance REST request/response structs
      ws_api.hpp          # Binance WebSocket API request/response structs + identify
      ws_streams.hpp      # Binance market stream types + identify_message
                          #   (listen-key user streams deferred)
      rest_client.hpp     # BinanceRestClient
      ws_client.hpp       # STREAM_URL/WS_API_URL consts + make_binance_stream_client()
                          #   / make_binance_ws_api_client() factories
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
    binance_rest_client_example.cpp    # analog of rest_client_example.cpp (BinanceRestClient)
    binance_ws_client_example.cpp      # analog of ws_client_example.cpp (Binance stream client)
```

> Fixture headers and the full test matrix are detailed in [001-appendix-testing-strategy.md](001-appendix-testing-strategy.md); their captured JSON comes from [001-appendix-binance-message-formats.md](001-appendix-binance-message-formats.md).

### Namespace layout after migration

| Namespace | Contains |
|---|---|
| `exchange::` | Canonical enums (Side, OrderType, TimeInForce, OrderStatus) |
| `exchange::rest::` | TypedPublicRequest<R>, TypedPrivateRequest<R>, RestResponse<T>, HttpRequest, IRestAuth |
| `exchange::ws::` | IWsConnection, **IxWsConnection** (generic transport), **WsReconnectSession**, WsResponse<T>, SubscriptionHandle, GenericWsClient, WsRequestBase, TypedWsRequest<R>, BaseWsResponse, `make_generic_ws_client()` |
| `exchange::kraken::` | Kraken-specific enums, TickPrice, OrderParams, Triggers, Conditional, OrderInfo, etc. |
| `exchange::kraken::rest::` | All Kraken REST req/resp types, Credentials, KrakenRestClient |
| `exchange::kraken::ws::` | All Kraken WS req/resp types, SubscribeChannel, WsCredentials, `identify_message`, URL consts, `make_kraken_ws_client()` |
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

### A2. WebSocket request scaffold — `TypedWsRequest<R>` and `TypedSubscribeRequest`

The dispatch loop above is only half the WS story. The request side has its own typed scaffold — the WS counterpart of `TypedPublicRequest<R>` / `TypedPrivateRequest<R>` on the REST side — and it generalises along the same lines: the *binding of request → response/push types* is exchange-agnostic and moves to `exchange::ws::`; only the *wire rendering* (`to_json`) stays per-exchange.

#### Method-call requests — `TypedWsRequest<R>`

Today this is literally `template<typename R> struct TypedWsRequest { using response_type = R; };`, and each request struct carries its own `req_id` field that `GenericWsClient::execute_async` assigns (`req.req_id = id`). It moves to `exchange::ws::` verbatim, with the per-struct `req_id` slot hoisted into a shared base so the assignment is uniform:

```cpp
// exchange/common/ws.hpp
namespace exchange::ws {

// Client-assigned correlation-id slot. Each exchange's to_json() renders it
// in whatever wire location/format that exchange uses (Kraken: "req_id" inside
// the message; Binance: top-level "id").
struct WsRequestBase { int64_t req_id{0}; };

template<typename R>
struct TypedWsRequest : WsRequestBase {
    using response_type = R;
};

} // namespace exchange::ws
```

**Contract that `GenericWsClient::execute_async<Req>` requires** (unchanged in substance, now the only thing an adapter must satisfy):
1. `Req` derives `WsRequestBase` (so the client can assign `req.req_id`).
2. `Req::response_type` names the reply type.
3. `Req::to_json() const -> json` — must serialise `req_id` into the exchange's correlation field.
4. `Req::response_type::from_json(const json&) -> Resp`.
5. `detail::make_ws_response(Resp)` derives `ok` (already type-dispatched: `success` for `BaseResponse` subtypes, `status<400` for Binance replies, always-true for plain `PongMessage`).

So `KrakenAddOrderRequest`, `BinanceWsNewOrderRequest`, and `BinanceWsPingRequest` are all just `: TypedWsRequest<TheirResponse>` with their own `to_json()`. No client changes.

#### Subscription requests — generalising `TypedSubscribeRequest`

This is the one place the current scaffold is genuinely Kraken-shaped and needs widening. Today:

```cpp
template<typename PushMsg, SubscribeChannel Ch>   // Ch is a Kraken-only enum
struct TypedSubscribeRequest : SubscribeRequest {
    using push_type     = PushMsg;
    using response_type = SubscribeResponse;       // Kraken-only ack type
    static constexpr SubscribeChannel channel_value = Ch;
    TypedSubscribeRequest() { this->channel = Ch; }
};
```

…and `subscribe_async` reaches into Kraken specifics: it computes the routing key with `to_string(req.channel)` and hand-builds a Kraken `UnsubscribeRequest`. Binance streams have **no fixed channel enum** — a stream is an open-ended `"<symbol>@<stream>"` string (`"btcusdt@aggTrade"`) — and a different ack shape (`{"result":null,"id":N}`), so neither `SubscribeChannel` nor `SubscribeResponse` fits.

**Generalisation**: keep the compile-time `push_type` / `response_type` binding (that is the type-safety win and it is exchange-neutral), but replace the three Kraken-specific touch-points with small member functions every subscribe request provides. The informal concept becomes:

```cpp
// A WS subscribe request models:
//   using push_type      = <push message type>;   // e.g. BinanceAggTradeEvent
//   using response_type  = <ack type>;            // from_json + make_ws_response → ok
//   int64_t req_id;                                // inherited from WsRequestBase
//   std::string route_key() const;                 // key the push callback registers under
//   json to_json() const;                          // the SUBSCRIBE frame
//   json unsubscribe_json() const;                 // the matching UNSUBSCRIBE frame
```

`GenericWsClient::subscribe_async` then becomes fully exchange-agnostic — the two Kraken-specific lines change to:

```cpp
const std::string key      = req.route_key();          // was: to_string(req.channel)
std::string       unsub    = req.unsubscribe_json().dump();  // was: hand-built UnsubscribeRequest
// ack handling routes through detail::make_ws_response(Ack::from_json(j)),
// so ws.ok is derived uniformly instead of reading ack.success directly.
```

Each exchange keeps its own typed alias layer on top. **Kraken** retains its enum internally and implements the concept in two one-line members:

```cpp
// exchange/kraken/ws_api.hpp
template<typename PushMsg, SubscribeChannel Ch>
struct TypedSubscribeRequest : SubscribeRequest {        // SubscribeRequest : WsRequestBase
    using push_type     = PushMsg;
    using response_type = SubscribeResponse;
    static constexpr SubscribeChannel channel_value = Ch;
    TypedSubscribeRequest() { this->channel = Ch; }
    std::string route_key() const { return to_string(channel); }   // "ticker"
    json        unsubscribe_json() const;                          // builds UnsubscribeRequest
    // to_json() inherited from SubscribeRequest
};
using TickerSubscribeRequest = TypedSubscribeRequest<TickerMessage, SubscribeChannel::Ticker>;
```

**Binance** models the same concept with a stream string instead of an enum:

```cpp
// exchange/binance/ws_streams.hpp
template<typename PushMsg>
struct TypedStreamSubscribeRequest : exchange::ws::WsRequestBase {
    using push_type     = PushMsg;
    using response_type = BinanceStreamAck;            // {"result":null,"id":N}
    std::string stream;                                 // "btcusdt@aggTrade"
    std::string route_key() const { return stream; }
    json to_json() const {
        return {{"method","SUBSCRIBE"}, {"params", json::array({stream})}, {"id", req_id}};
    }
    json unsubscribe_json() const {
        return {{"method","UNSUBSCRIBE"}, {"params", json::array({stream})}, {"id", req_id}};
    }
};
using BinanceAggTradeSubscribe = TypedStreamSubscribeRequest<BinanceAggTradeEvent>;
```

#### Request-side concept across both exchanges

| Concept | Kraken | Binance streams | Binance WS API |
|---|---|---|---|
| Method-call base | `TypedWsRequest<R>` | — (streams have no method calls) | `TypedWsRequest<R>` |
| Correlation slot | `req_id` → `"req_id"` in msg | `req_id` → top-level `"id"` | `req_id` → top-level `"id"` |
| Method name | `"add_order"`, `"ping"` | `"SUBSCRIBE"` | `"order.place"`, `"ping"` |
| Subscribe base | `TypedSubscribeRequest<PushMsg, Ch>` | `TypedStreamSubscribeRequest<PushMsg>` | — (no push channel) |
| `route_key()` | `to_string(channel)` → `"ticker"` | `stream` → `"btcusdt@aggTrade"` | — |
| Ack type (`response_type`) | `SubscribeResponse` (`success` flag) | `BinanceStreamAck` (`result==null`) | — |
| `unsubscribe_json()` | `UnsubscribeRequest` frame | `UNSUBSCRIBE` frame | — |

Net effect: `TypedWsRequest<R>` moves to `exchange::ws::` unchanged; `TypedSubscribeRequest` loses its three Kraken-specific touch-points to `route_key()` / `unsubscribe_json()` / a `from_json`-able ack type, after which a single `GenericWsClient::subscribe_async` drives Kraken channels and Binance streams identically.

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

### E. Transport and reconnect — generic core, narrow per-exchange seams

`IxWsConnection` (the ixwebsocket implementation of `IWsConnection`) and `WsReconnectSession` (the reconnect/backoff thread) look exchange-specific because they live in `kraken_*`/`kraken::ws::` today, but reading them shows **neither contains any Kraken protocol logic**:

- `IxWsConnection` is a pure transport adapter — it forwards ixwebsocket's open/close/message/error callbacks to the `IWsConnection` interface. Its only Kraken coupling is the namespace and a factory that returns `KrakenWsClient`.
- `WsReconnectSession` is pure machinery — a background thread, a mutex/cv, exponential backoff, and two caller-supplied callbacks (`ConnectFn`, `DisconnectFn`). Its own header already describes it as "transport-level." Zero protocol awareness.

Both therefore move to **`exchange/common/`** (namespace `exchange::ws::`), not into either exchange. The generic URL-string factory becomes:

```cpp
// exchange/common/ix_ws_connection.hpp
inline std::shared_ptr<GenericWsClient>
make_generic_ws_client(const std::string&               url,
                       MessageIdentifier                identifier,
                       std::shared_ptr<IWsErrorHandler> error_handler = nullptr) {
    auto conn   = std::make_shared<IxWsConnection>(url);
    auto client = std::make_shared<GenericWsClient>(conn, std::move(identifier),
                                                    std::move(error_handler));
    client->init();
    conn->connect();
    return client;
}
```

Each exchange then supplies a one-line wrapper that binds its own `identify_message` and URL constant (`make_kraken_ws_client`, `make_binance_stream_client`, `make_binance_ws_api_client`).

**The genuine per-exchange seams are narrow and data-shaped, not logic-shaped:**

| Seam | Kraken | Binance | Where it lives |
|---|---|---|---|
| Endpoint URLs | `PUBLIC_WS_URL`, `PRIVATE_WS_URL` | `STREAM_URL`, `WS_API_URL` | per-exchange `ws_client.hpp` consts |
| Client factory | `make_kraken_ws_client` | `make_binance_stream_client` / `…ws_api_client` | per-exchange `ws_client.hpp` |
| Frame routing | `identify_message` (channel/req_id) | `identify_message` (stream/id) | per-exchange `ws_api.hpp` / `ws_streams.hpp` |
| Keepalive | app-level `{"channel":"heartbeat"}` frame (informational; already classified as `MessageKind::Heartbeat`) | RFC 6455 protocol ping → ixwebsocket auto-pongs within the 60 s window | transport (ixwebsocket) handles both for free |
| Reconnect trigger | on socket close | on socket close **and** Binance's mandatory ~24 h server-side disconnect | same `WsReconnectSession`; Binance just disconnects more often |
| Resubscribe-on-reconnect | re-send Kraken subscribe frames | re-send `SUBSCRIBE` for each stream | caller's `ConnectFn` (per-exchange, supplied at the call site) |

Two points worth calling out:
1. **Binance keepalive is free.** Binance requires the client to answer the server's 20 s ping with a pong inside 60 s; ixwebsocket auto-responds to protocol ping frames per RFC 6455, so no code is needed. `IWsConnection` deliberately has no ping method — keepalive stays a transport concern.
2. **Reconnect resubscription is the caller's job, by design.** `WsReconnectSession` stays generic because *what to re-subscribe* is supplied through `ConnectFn`. The Binance `binance_ws_client_example` and the Kraken examples each show their own resubscribe set; the session itself is identical. (An optional future convenience — a `GenericWsClient::resubscribe_all()` that replays its active `subscriptions_` — would live in common too, but is out of scope here.)

Optional, generic (not exchange-specific) extension: give `IxWsConnection`'s constructor optional ping-interval / extra-handshake-header parameters. Not required for Kraken or Binance, but it keeps future exchanges that need handshake auth headers from touching the interface.

---

## Steps

### Step 1 — Create common scaffold headers (no logic change)

**Done when**: New headers exist; existing code compiles after `#include` path update.

- Create `include/exchange/common/rest.hpp` — copy `TypedPublicRequest<R>`, `TypedPrivateRequest<R>`, `RestResponse<T>`, `HttpRequest`, `IRestAuth` (stub) from `kraken_rest_api.hpp`/`kraken_rest_client.hpp`. Namespace: `exchange::rest::`.
- Create `include/exchange/common/ws.hpp` — copy `IWsConnection`, `WsResponse<T>`, `SubscriptionHandle`, `BaseWsResponse`, and the request-side scaffold `WsRequestBase` + `TypedWsRequest<R>` (see §A2) from `kraken_ws_client.hpp`/`kraken_ws_api.hpp`. Namespace: `exchange::ws::`. Document the subscribe-request concept (`push_type`, `response_type`, `route_key()`, `to_json()`, `unsubscribe_json()`) as a comment here — it is structural, not a base class.
- Create `include/exchange/common/ws_client.hpp` + `.inl` — `GenericWsClient` taking `MessageIdentifier`. `subscribe_async` calls `req.route_key()` / `req.unsubscribe_json()` and routes the ack through `detail::make_ws_response(Ack::from_json(j))` (no `to_string(channel)` / hand-built `UnsubscribeRequest` / direct `ack.success` read). Namespace: `exchange::ws::`.
- Create `include/exchange/common/ix_ws_connection.hpp` — move `IxWsConnection` here verbatim (it has no Kraken logic; see §E), namespace `exchange::ws::`, and add the generic `make_generic_ws_client(url, identifier, error_handler)` factory.
- Create `include/exchange/common/reconnect_session.hpp` (+ `.inl`) — move `WsReconnectSession` here verbatim (generic machinery; see §E), namespace `exchange::ws::`.
- Create `include/exchange/common/types.hpp` — `Side`, `OrderType`, `TimeInForce`, `OrderStatus` (extracted from `kraken_types.hpp`). Namespace: `exchange::`.
- No files deleted yet; the existing `kraken_ws_client.hpp`, `kraken_ix_ws_connection.hpp`, and `ws_reconnect_session.hpp` become thin re-export shims (`using exchange::ws::IxWsConnection;` etc.) so current code and tests keep compiling unchanged.
- **Tests**: Full build + all existing tests pass. No behaviour change.

### Step 2 — Migrate Kraken to `exchange/kraken/` namespaces

**Done when**: All Kraken code lives under `exchange::kraken::*`; all tests pass.

- Create `include/exchange/kraken/auth.hpp` — move `Credentials`, `sign()`, `make_nonce()`, crypto helpers. Namespace: `exchange::kraken::rest::`.
- Create `include/exchange/kraken/types.hpp` — Kraken-specific types (PriceType, TriggerReference, StpType, FeePreference, TickPrice, OrderParams, Triggers, Conditional, OrderDescription, OrderInfo, TradeInfo, LedgerEntry). Namespace: `exchange::kraken::`.
- Create `include/exchange/kraken/rest_api.hpp` — all REST req/resp structs, now using `exchange::kraken::rest::` namespace.
- Create `include/exchange/kraken/ws_api.hpp` — all WS req/resp structs, `exchange::kraken::ws::`. Adapt the WS request scaffold to the generalised contract (§A2): method-call requests inherit `exchange::ws::TypedWsRequest<R>` (their per-struct `req_id` slot now comes from `WsRequestBase`); `TypedSubscribeRequest<PushMsg, Ch>` gains `route_key()` (`return to_string(channel)`) and `unsubscribe_json()` (builds the existing `UnsubscribeRequest`). `SubscribeResponse` keeps `success`; verify `make_ws_response(SubscribeResponse)` derives `ok` from it.
- Create `include/exchange/kraken/rest_client.hpp` — `KrakenRestClient` wraps `GenericRestClient` + `KrakenAuth : IRestAuth`.
- Create `include/exchange/kraken/ws_client.hpp` — Kraken WS endpoint constants (`PUBLIC_WS_URL`, `PRIVATE_WS_URL`) plus a one-line `make_kraken_ws_client(url, error_handler)` that calls the common `make_generic_ws_client(url, kraken::ws::identify_message, …)`. No `IxWsConnection`/`WsReconnectSession` copy here — those are the common headers from Step 1, reused as-is.
- Update `src/kraken_*.cpp` to use new headers/namespaces.
- Delete old `include/kraken_*.hpp` files **and the Step 1 re-export shims** (`kraken_ix_ws_connection.hpp`, `ws_reconnect_session.hpp`) once all includes are updated.
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
- Add `examples/binance_rest_client_example.cpp` — the direct analog of `rest_client_example.cpp`: a CLI11 app with one subcommand per public endpoint (`ping`, `time`, `exchangeinfo`, `ticker [--symbols …]`, `book <symbol> [--limit N]`, `klines <symbol> --interval 1m`, `trades <symbol> [--limit N]`), each `run_*(BinanceRestClient&, args)` executing the typed request and logging the parsed fields via spdlog. `main()` mirrors the Kraken example: `curl_global_init` → construct `BinanceRestClient` → dispatch by subcommand → `curl_global_cleanup`. Public endpoints only — no credentials. Links `binanceapi spdlog::spdlog CLI11::CLI11 example_backward`.
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
  - `TypedStreamSubscribeRequest<PushMsg>` (§A2) — stores the `"<symbol>@<stream>"` string; `route_key()` returns it; `to_json()`/`unsubscribe_json()` emit `{"method":"SUBSCRIBE|UNSUBSCRIBE","params":[stream],"id":req_id}`. `response_type = BinanceStreamAck` (parses `{"result":null,"id":N}`; `make_ws_response` derives `ok` from result-present / no-error). Per-stream aliases: `BinanceAggTradeSubscribe`, `BinanceTradeSubscribe`, `BinanceKlineSubscribe`, etc.
  - The subscribe ack carries **no stream echo**, so `SubscriptionHandle` remembers the `route_key` from the request — the generic client already supports this (the handle stores its own key), so no client change is needed.
- `make_binance_stream_client(url)` factory — one-liner over the common `make_generic_ws_client(url, BinanceStreamIdentifier)`. No new transport or reconnect code: `IxWsConnection` and `WsReconnectSession` are reused unchanged from `exchange/common/` (see §E). ixwebsocket auto-pongs Binance's 20 s server pings; `WsReconnectSession` handles the mandatory ~24 h reconnect, with the resubscribe set supplied by the caller's `ConnectFn`.
- Create `tests/unit/binance_ws_stream_example_json.hpp` — captured push frames + subscribe ack (the direct analog of `ws_client_example_json.hpp`).
- Create `tests/unit/test_binance_ws_client.cpp` — `identify_message` tests (one per event type), `from_json` field assertions, and `MockWsConnection` subscribe-lifecycle tests (fire_open → subscribe ack by id → inject push frame → callback fires → cancel).
- Add `examples/binance_ws_client_example.cpp` — the direct analog of `ws_client_example.cpp`: a CLI11 app with one subcommand per stream (`aggtrade <symbol>`, `trade <symbol>`, `kline <symbol> --interval 1m`, `ticker <symbol>`, `miniticker <symbol>`, `bookticker <symbol>`, `depth <symbol> [--levels N]`), each `run_*()` creating a client via `make_binance_stream_client(STREAM_URL)`, subscribing with the typed `TypedStreamSubscribeRequest` + a push callback that logs frames, then unsubscribing via the handle. Mirror the Kraken example's **connection-reuse demo** with the Binance-natural equivalent: subscribe to *several streams on one client* over the single combined-stream connection (e.g. `aggTrade` + `bookTicker` for the same symbol), showing multiple active `SubscriptionHandle`s sharing one socket. Public streams only — no credentials. Links `binanceapi ixwebsocket spdlog::spdlog CLI11::CLI11 example_backward`.
- **Tests**: All unit tests pass.

### Step 7 — Binance WebSocket API (bidirectional trading)

**Done when**: `BinanceWsClient` can place and cancel orders over WebSocket.

- Create `include/exchange/binance/ws_api.hpp`:
  - `BinanceWsCredentials` — per-request signing: `apiKey` + `signature` + `timestamp` inside `params` (the logon-session flow is deferred). Reuses `BinanceAuth`'s HMAC-SHA256 over the sorted `params`.
  - Request/response pairs, each inheriting `exchange::ws::TypedWsRequest<R>` (§A2) with its own `to_json()` rendering `req_id` as the top-level `"id"`: `BinanceWsNewOrderRequest → BinanceWsNewOrderResponse` (`method:"order.place"`), `BinanceWsCancelOrderRequest → BinanceWsCancelOrderResponse` (`method:"order.cancel"`), `BinanceWsPingRequest → BinanceWsPongMessage` (`method:"ping"`).
  - **Format specifics**: request is `{"id":"<uuid|int>","method":"…","params":{…}}`; success reply `{"id":…,"status":200,"result":{…},"rateLimits":[…]}`; error reply `{"id":…,"status":400,"error":{"code":-2010,"msg":"…"}}`. `BinanceWsIdentifier` classifies every reply as a `MethodResponse` with `correlation_id = stringify(id)` — there is no `channel`/push concept on the WS API endpoint. `ok` is derived from `status < 400`; on error, populate `WsResponse::error` from `error.msg`.
  - The `id` is generated as an `int64_t` and stringified (the generic client's `correlation_id` is already a string), matching the Kraken adapter's approach.
- `make_binance_ws_api_client(url)` factory (endpoint `wss://ws-api.binance.com/ws-api/v3`) — again a one-liner over the common `make_generic_ws_client`; transport/reconnect reused from `exchange/common/` (§E).
- Create `tests/unit/binance_ws_api_example_json.hpp` fixtures (success + error replies) and add `identify_message` + `from_json` + `MockWsConnection` execute-lifecycle tests to `test_binance_ws_client.cpp`.
- **Tests**: All unit tests pass.

### Step 8 — CMake, build validation, final cleanup

**Done when**: Full build from clean with all tests passing; examples compile; CI-equivalent local check.

- Update `src/CMakeLists.txt`:
  - `krakenapi::krakenapi` target — Kraken sources only (existing).
  - New `krakenapi::binanceapi` target — Binance sources + links `krakenapi::krakenapi` for common scaffold.
  - Common scaffold (`include/exchange/common/`) is header-only; no separate lib.
- Update top-level `CMakeLists.txt` to `add_subdirectory` any new source directories.
- Update `tests/CMakeLists.txt` to add Binance unit test executables **and the two example executables**, mirroring the existing `rest_client_example` / `ws_client_example` wiring:
  - `binance_rest_client_example` → `target_link_libraries(… binanceapi spdlog::spdlog CLI11::CLI11 example_backward)`
  - `binance_ws_client_example` → `target_link_libraries(… binanceapi ixwebsocket spdlog::spdlog CLI11::CLI11 example_backward)` (ixwebsocket for the real transport, as the Kraken WS example does)
- Remove any dead code or stale comments from the refactor.
- Run `ctest --output-on-failure` — all tests pass.
- Verify **all** examples compile against the restructured headers (both Kraken and the two new Binance examples).
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
