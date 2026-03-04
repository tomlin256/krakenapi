# CLAUDE.md — krakenapi

A type-safe C++ library for the [Kraken](https://kraken.com) Spot REST and WebSocket v2 APIs.

---

## Project structure

```
krakenapi/
├── CMakeLists.txt                   # Top-level build; fetches all external deps
├── include/
│   ├── kraken_types.hpp             # Shared enums, structs, and RestResponse<T>
│   ├── kraken_rest_api.hpp          # All REST request/response types + HMAC signing
│   ├── kraken_rest_client.hpp       # KrakenRestClient — typed libcurl executor
│   ├── kraken_ws_api.hpp            # WebSocket v2 request/response types
│   ├── kraken_ws_client.hpp         # KrakenWsClient + IWsConnection interface
│   ├── kraken_ws_client.inl         # Template method implementations (included by .hpp)
│   └── kraken_ix_ws_connection.hpp  # IxWsConnection (ixwebsocket) + URL factory overload
├── src/
│   ├── CMakeLists.txt               # Builds libkrakenapi.a; exports krakenapi::krakenapi
│   ├── kraken_rest_client.cpp       # KrakenRestClient implementation
│   └── kraken_ws_client.cpp         # KrakenWsClient non-template implementations
└── tests/
    ├── CMakeLists.txt               # Fetches spdlog; wires examples + unit tests
    ├── examples/
    │   ├── public_rest.cpp          # Fetch recent trades (no credentials)
    │   ├── private_rest.cpp         # Get WS token from ~/.kraken/default
    │   ├── public_ws.cpp            # Subscribe to ticker over WS (low-level)
    │   ├── private_ws.cpp           # Subscribe to balances over authenticated WS
    │   ├── ws_client_example.cpp    # KrakenWsClient ticker subscription + connection reuse demo
    │   ├── kraken_example.cpp       # REST + WebSocket combined demo
    │   └── kapi.hpp / kapi.cpp      # Legacy KAPI reference wrapper (not installed)
    └── unit/
        ├── CMakeLists.txt
        ├── test_signature.cpp       # HMAC-SHA512 output vs. reference KAPI impl
        ├── test_rest_requests.cpp
        ├── test_rest_responses.cpp
        ├── test_client.cpp          # Full execute() cycle with mock HTTP performer
        └── test_ws_client.cpp       # KrakenWsClient lifecycle with MockWsConnection
```

---

## Build system

- **CMake 3.15+**, **C++17** required.
- **System dependencies** (must be installed before configuring):
  - OpenSSL (`libssl-dev`)
  - libcurl (`libcurl4-openssl-dev`)
- **Fetched automatically** by `FetchContent` at configure time:
  | Library | Version | Used by |
  |---|---|---|
  | IXWebSocket | v11.4.6 | WebSocket examples + `IxWsConnection` |
  | nlohmann/json | v3.12.0 | All JSON parsing |
  | spdlog | v1.17.0 | Examples and tests |
  | Google Test | v1.16.0 | Unit tests |

### Common build commands

```bash
# Install system deps (Debian/Ubuntu)
sudo apt install libssl-dev libcurl4-openssl-dev

# Configure + build (debug, tests enabled by default)
cmake -B build
cmake --build build

# Release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Skip tests and examples
cmake -B build -DKRAKENAPI_BUILD_TESTS=OFF
cmake --build build
```

### Build outputs

| Path | Description |
|---|---|
| `build/src/libkrakenapi.a` | Static library — link this in your project |
| `build/bin/public_rest` | Public REST demo |
| `build/bin/private_rest` | Private REST demo |
| `build/bin/public_ws` | Public WebSocket demo (low-level) |
| `build/bin/private_ws` | Private WebSocket demo |
| `build/bin/ws_client_example` | `KrakenWsClient` subscription + connection reuse demo |
| `build/bin/kraken_example` | Combined REST + WebSocket demo |

---

## Running tests

```bash
cd build && ctest --output-on-failure
```

There are two test executables:

| Binary | Source | What it tests |
|---|---|---|
| `build/bin/kraken_unit_tests` | `tests/unit/` (REST tests) | REST request building, response parsing, signature, HTTP mock |
| `build/bin/test_ws_client` | `tests/unit/test_ws_client.cpp` | `KrakenWsClient` lifecycle with `MockWsConnection` |

Tests do **not** require network access or credentials — all I/O is mocked.

### Test suite breakdown

| File | What it verifies |
|---|---|
| `test_signature.cpp` | `Credentials::sign()` produces byte-identical output to the legacy `KAPI::signature()` for the same inputs |
| `test_rest_requests.cpp` | Each request type builds the correct HTTP path, method, query string, and body |
| `test_rest_responses.cpp` | JSON deserialization is correct for every response type |
| `test_client.cpp` | `KrakenRestClient::execute()` round-trips public and private requests end-to-end using an injected mock performer |
| `test_ws_client.cpp` | `KrakenWsClient` execute/subscribe lifecycle, timeout, pre-connection queuing, `SubscriptionHandle::cancel()` idempotency |

---

## Namespace layout

| Namespace | Location | Contains |
|---|---|---|
| `kraken::` | `kraken_types.hpp` | Shared enums (`OrderType`, `Side`, `TimeInForce`, …), shared structs (`OrderParams`, `OrderInfo`, `TradeInfo`, `LedgerEntry`), generic envelope `RestResponse<T>`, `parse_rest_response<T>()` |
| `kraken::rest::` | `kraken_rest_api.hpp`, `kraken_rest_client.hpp` | All REST request/response types, `Credentials`, `HttpRequest`, `KrakenRestClient` |
| `kraken::ws::` | `kraken_ws_api.hpp`, `kraken_ws_client.hpp`, `kraken_ix_ws_connection.hpp` | All WebSocket v2 request/response types, `KrakenWsClient`, `IWsConnection`, `IxWsConnection`, `SubscriptionHandle`, `WsResponse<T>` |

---

## Architecture and key patterns

### REST layer

Every public REST endpoint follows the **TypedPublicRequest<R>** pattern:

```cpp
struct GetServerTimeRequest : PublicRequest {
    using response_type = ServerTime;           // links request → result type
    HttpRequest build() const;                  // produces path + query string
};
```

Every private REST endpoint follows **TypedPrivateRequest<R>**:

```cpp
struct GetAccountBalanceRequest : PrivateRequest {
    using response_type = AccountBalanceResult;
    HttpRequest build(const Credentials&) const; // adds nonce + HMAC signature
};
```

`KrakenRestClient::execute()` is templated on the request type. The compiler resolves `Req::response_type` at call-site, giving end-to-end type safety without any casts:

```cpp
auto resp = client.execute(GetServerTimeRequest{});
// resp is RestResponse<ServerTime>
```

### REST authentication (private endpoints)

1. Generate a nonce — a monotonically increasing `uint64` (millisecond timestamp is used by default).
2. Build the POST body as `nonce=<value>[&extra_params]`.
3. Compute signature:
   ```
   msg    = URI_path + SHA256(nonce_string + POST_body)
   sign   = HMAC-SHA512(base64_decode(api_secret), msg)
   header = base64_encode(sign)
   ```
4. Send headers `API-Key` and `API-Sign` with the POST request.

`Credentials::sign(path, nonce_str, postdata)` in `kraken_rest_api.hpp` implements this. Unit tests in `test_signature.cpp` verify it matches the legacy reference implementation byte-for-byte.

### WebSocket layer — `KrakenWsClient`

`KrakenWsClient` provides a type-safe wrapper over a raw WebSocket connection. It mirrors the REST client's typed request/response pattern with two operation modes:

#### Method calls (single request → single response)

```cpp
// Blocking – waits up to timeout (default 5 s)
auto resp = client->execute(PingRequest{});              // WsResponse<PongMessage>

// Non-blocking – returns std::future
auto fut  = client->execute_async(AddOrderRequest{…});  // future<WsResponse<AddOrderResponse>>
```

#### Subscriptions (three-phase lifecycle)

```cpp
// Blocking – waits for the server ack (Phase 3)
auto [ack, handle] = client->subscribe(
    sub_req,
    [](TickerMessage msg) { /* push callback */ },
    std::chrono::milliseconds{10000}
);
if (!ack.ok) { /* handle error */ }

handle.cancel();  // unsubscribes; idempotent
```

**Three phases:**
1. **Phase 1** — WebSocket connection opens (`on_open` fires). Requests made before `on_open` are queued internally and flushed atomically when the socket opens.
2. **Phase 2** — `SubscribeRequest` is sent (with an auto-assigned unique `req_id`).
3. **Phase 3** — `SubscribeResponse` ack received and matched by `req_id`.
   - Success: push callback installed in the dispatch table; `SubscriptionHandle` is active.
   - Failure: push callback never installed; `SubscriptionHandle` is inactive.

Incoming server frames are dispatched to either a pending handler (matched by `req_id`) or an active push subscription callback (matched by the `"channel"` field).

#### Connection abstraction

`IWsConnection` is a pure abstract interface with no ixwebsocket symbols visible to callers:

```cpp
class IWsConnection {
public:
    virtual void connect()                    = 0;
    virtual void disconnect()                 = 0;
    virtual bool is_connected() const         = 0;
    virtual void send(const std::string& msg) = 0;
    virtual void on_message(MessageCb cb)     = 0;
    virtual void on_open(OpenCb cb)           = 0;
    virtual void on_close(CloseCb cb)         = 0;
};
```

`IxWsConnection` (in `kraken_ix_ws_connection.hpp`) implements this using ixwebsocket. Unit tests inject `MockWsConnection` instead — no network required.

#### File split: `.hpp` / `.inl` / `.cpp`

| File | Contents |
|---|---|
| `kraken_ws_client.hpp` | Class declarations, `IWsConnection`, `SubscriptionHandle`, `WsResponse<T>` |
| `kraken_ws_client.inl` | Template method bodies (`execute`, `execute_async`, `subscribe`, `subscribe_async`) — `#include`d at the bottom of the `.hpp` |
| `src/kraken_ws_client.cpp` | Non-template method bodies (`init`, `on_open_handler`, `on_raw_message`, `cancel_subscription`, `enqueue_or_send`) |
| `kraken_ix_ws_connection.hpp` | `IxWsConnection` + URL-string factory overload of `make_ws_client()` |

Include only `kraken_ws_client.hpp` for test/mock usage. Include `kraken_ix_ws_connection.hpp` when you need the real ixwebsocket transport.

#### Factory functions

```cpp
// Wraps an already-managed connection (useful for mocks or connection reuse).
// Defined in kraken_ws_client.inl.
std::shared_ptr<KrakenWsClient> make_ws_client(std::shared_ptr<IWsConnection> conn);

// Creates a fresh IxWsConnection, calls init() and connect().
// Defined in kraken_ix_ws_connection.hpp — include that header to use this overload.
std::shared_ptr<KrakenWsClient> make_ws_client(const std::string& url);
```

Both factories call `client->init()` — never call `init()` yourself.

### WebSocket authentication

Private WebSocket channels use a **session token** (not the API key/secret directly):

1. Call `GetWebSocketsTokenRequest` via the REST client to obtain a token.
2. Pass the token as the `"token"` field inside each WebSocket request's `params`.

`WsCredentials` wraps the token; `AddOrderRequest`, `SubscribeRequest`, etc. accept it directly.

### `WsResponse<T>`

Mirrors `RestResponse<T>` for the WebSocket layer:

```cpp
template<typename T>
struct WsResponse {
    bool ok{false};
    std::optional<std::string> error;
    std::optional<T>           result;
};
```

`ok` is derived from `BaseResponse::success` for response types that inherit `BaseResponse`; for plain types like `PongMessage` it is always `true`.

### Generic REST response envelope

The Kraken REST API wraps every response in `{ "error": [], "result": <T> }`. The helper:

```cpp
template<typename T>
RestResponse<T> parse_rest_response(const json& j);
```

…handles this universally. `RestResponse<T>` has three fields:
- `bool ok` — true when `error` array is empty
- `std::optional<T> result` — populated on success
- `std::vector<std::string> errors` — populated on failure

Always check `resp.ok` before accessing `resp.result`.

### WebSocket message dispatch (low-level)

For callers that bypass `KrakenWsClient` and handle raw frames themselves:

```cpp
auto kind = kraken::ws::identify_message(json);
switch (kind) {
    case kraken::ws::MessageKind::Ticker:
        auto m = kraken::ws::TickerMessage::from_json(json);
        break;
    // ...
}
```

`identify_message()` inspects the `"method"` key for replies and the `"channel"` key for push messages.

---

## Adding a new REST endpoint

1. **Declare request and response types** in `kraken_rest_api.hpp`:
   - Public: inherit `PublicRequest`, define `using response_type = YourResult`, implement `build()`.
   - Private: inherit `PrivateRequest`, define `using response_type = YourResult`, implement `build(const Credentials&)`.
   - Add `YourResult::from_json(const json&)` as a static method.

2. **Add a unit test** in `tests/unit/test_rest_requests.cpp` verifying the path, method, and body fields, and in `test_rest_responses.cpp` verifying JSON deserialization.

3. No changes needed to `KrakenRestClient` — it is fully templated.

---

## Adding a new WebSocket method call

1. Add a request struct in `kraken_ws_api.hpp`:
   - `using response_type = YourResponse;`
   - `int64_t req_id{0};` field (set automatically by `KrakenWsClient`).
   - `json to_json() const` — must include `req_id` in the output.
2. Add `YourResponse` with `static YourResponse from_json(const json&)`.
   - If the server returns `success`/`error` fields, inherit `BaseResponse`.
3. Add the new `MessageKind` enum value and handle it in `identify_message()`.
4. Add unit tests in `test_ws_client.cpp` using `MockWsConnection`.

---

## Adding a new WebSocket subscription

1. Add a subscribe request struct in `kraken_ws_api.hpp`:
   - `using response_type = SubscribeResponse;`
   - `using push_type = YourPushMessage;`
   - `SubscribeChannel channel;` field.
   - Optional: `std::vector<std::string> symbols` and `std::optional<std::string> token`.
   - `json to_json() const` method.
2. Add `YourPushMessage` with `static YourPushMessage from_json(const json&)`.
3. Add the channel string mapping in `to_string(SubscribeChannel)`.
4. Add unit tests in `test_ws_client.cpp`.

---

## Adding a new low-level WebSocket message type

1. Add request struct with `json to_json() const` and response struct with `static T from_json(const json&)` in `kraken_ws_api.hpp`.
2. Add the new `MessageKind` enum value.
3. Handle the new method name or channel string in `identify_message()`.

---

## Testing without network

### REST (mock HTTP performer)

Unit tests inject a custom HTTP performer via `make_test_client`:

```cpp
auto client = make_test_client([](const kraken::rest::HttpRequest& http) -> std::string {
    // inspect http.path, http.method, http.body, http.headers
    return R"({"error":[],"result":{...}})";
});
auto resp = client.execute(SomeRequest{});
```

This factory is declared `inline` in `kraken_rest_client.hpp` and friends `KrakenRestClient`'s private constructor, so no changes to the library source are needed.

### WebSocket (MockWsConnection)

Unit tests create a `MockWsConnection` (defined in `test_ws_client.cpp`) and inject it via `make_ws_client(conn)`:

```cpp
auto conn   = std::make_shared<MockWsConnection>();
auto client = kraken::ws::make_ws_client(
                  std::static_pointer_cast<kraken::ws::IWsConnection>(conn));

conn->fire_open();            // simulate connection open
conn->sent_messages;          // inspect outbound messages (std::vector<std::string>)
conn->inject_message(raw);    // inject inbound server frame
conn->fire_close();           // simulate disconnect
```

`MockWsConnection::connect()` does **not** auto-fire `on_open` — tests call `fire_open()` explicitly, enabling precise control over the pre-connection outbound queue.

---

## Credentials file format

Private example programs load credentials from `~/.kraken/<name>` (default: `~/.kraken/default`):

```
<api_key>
<base64_encoded_private_key>
```

Line 1: API public key string.
Line 2: Base64-encoded private key (as provided by Kraken).

Load in code:
```cpp
auto creds = kraken::rest::Credentials::from_file("default");
```

---

## Coding conventions

- **C++17** throughout; use structured bindings, `if constexpr`, `std::optional`, `std::string_view` where appropriate.
- All optional fields on request and response structs use `std::optional<T>`. Only set them when needed; omitted fields are not serialised.
- JSON serialisation uses `to_json()` / `from_json()` static methods on each struct. Prefer `j.value("key", default)` over `j.at("key")` for fields that may be absent in responses.
- Enum conversions are done by free functions `to_string(Enum)` and `foo_from_string(const std::string&)` in `kraken_types.hpp`. These throw `std::invalid_argument` on unknown values.
- Monetary / volume fields returned by the REST API arrive as JSON **strings** (e.g., `"1.5"`) — deserialise with `std::stod(j.value("field", "0"))` rather than `.get<double>()`.
- The static library (`libkrakenapi.a`) links against libcurl and OpenSSL. It also compiles `kraken_ws_client.cpp`, but does **not** link against ixwebsocket. Callers that use `IxWsConnection` must separately link against `ixwebsocket`.
- IXWebSocket and spdlog are **not** linked into `libkrakenapi.a`; they are used only by examples and tests.
- Template methods for `KrakenWsClient` live in `kraken_ws_client.inl` (included at the bottom of `.hpp`). Non-template methods live in `src/kraken_ws_client.cpp`. Keep this split consistent when adding new methods.

---

## WebSocket endpoints

| Endpoint | URL |
|---|---|
| Public | `wss://ws.kraken.com/v2` |
| Private (authenticated) | `wss://ws-auth.kraken.com/v2` |

URL constants are available as `kraken::ws::PUBLIC_WS_URL` and `kraken::ws::PRIVATE_WS_URL` (declared in `kraken_ws_client.hpp`).

The private endpoint requires a token obtained via `GetWebSocketsTokenRequest` before connecting.

---

## Running examples

```bash
# Public (no credentials needed)
./build/bin/public_rest
./build/bin/public_ws BTC/EUR
./build/bin/ws_client_example BTC/USD   # KrakenWsClient subscription + connection reuse

# Private (requires ~/.kraken/default)
./build/bin/private_rest
./build/bin/private_ws

# Combined REST + WS demo
./build/bin/kraken_example
```

Callers that embed the REST client must call `curl_global_init(CURL_GLOBAL_ALL)` before constructing `KrakenRestClient` and `curl_global_cleanup()` on teardown.
