# CLAUDE.md — krakenapi

A type-safe C++ library for the [Kraken](https://kraken.com) Spot REST and WebSocket v2 APIs.

---

## Project structure

```
krakenapi/
├── CMakeLists.txt               # Top-level build; fetches all external deps
├── include/
│   ├── kraken_types.hpp         # Shared enums, structs, and RestResponse<T>
│   ├── kraken_rest_api.hpp      # All REST request/response types + HMAC signing
│   ├── kraken_rest_client.hpp   # KrakenRestClient — typed libcurl executor
│   └── kraken_ws_api.hpp        # WebSocket v2 request/response types
├── src/
│   ├── CMakeLists.txt           # Builds libkrakenapi.a; exports krakenapi::krakenapi
│   └── kraken_rest_client.cpp   # KrakenRestClient implementation
└── tests/
    ├── CMakeLists.txt           # Fetches spdlog; wires examples + unit tests
    ├── examples/
    │   ├── public_rest.cpp      # Fetch recent trades (no credentials)
    │   ├── private_rest.cpp     # Get WS token from ~/.kraken/default
    │   ├── public_ws.cpp        # Subscribe to ticker over WS
    │   ├── private_ws.cpp       # Subscribe to balances over authenticated WS
    │   ├── kraken_example.cpp   # REST + WebSocket combined demo
    │   ├── kapi.hpp / kapi.cpp  # Legacy KAPI reference wrapper (not installed)
    └── unit/
        ├── test_signature.cpp   # HMAC-SHA512 output vs. reference KAPI impl
        ├── test_rest_requests.cpp
        ├── test_rest_responses.cpp
        └── test_client.cpp      # Full execute() cycle with mock HTTP performer
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
  | IXWebSocket | v11.4.6 | WebSocket examples |
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
| `build/bin/public_ws` | Public WebSocket demo |
| `build/bin/private_ws` | Private WebSocket demo |
| `build/bin/kraken_example` | Combined REST + WebSocket demo |

---

## Running tests

```bash
cd build && ctest --output-on-failure
```

The unit test binary is `build/bin/kraken_unit_tests` (or wherever `gtest_discover_tests` places it). Tests do **not** require network access or credentials — all HTTP calls are mocked.

### Test suite breakdown

| File | What it verifies |
|---|---|
| `test_signature.cpp` | `Credentials::sign()` produces byte-identical output to the legacy `KAPI::signature()` for the same inputs |
| `test_rest_requests.cpp` | Each request type builds the correct HTTP path, method, query string, and body |
| `test_rest_responses.cpp` | JSON deserialization is correct for every response type |
| `test_client.cpp` | `KrakenRestClient::execute()` round-trips public and private requests end-to-end using an injected mock performer |

---

## Namespace layout

| Namespace | Location | Contains |
|---|---|---|
| `kraken::` | `kraken_types.hpp` | Shared enums (`OrderType`, `Side`, `TimeInForce`, …), shared structs (`OrderParams`, `OrderInfo`, `TradeInfo`, `LedgerEntry`), generic envelope `RestResponse<T>`, `parse_rest_response<T>()` |
| `kraken::rest::` | `kraken_rest_api.hpp`, `kraken_rest_client.hpp` | All REST request/response types, `Credentials`, `HttpRequest`, `KrakenRestClient` |
| `kraken::ws::` | `kraken_ws_api.hpp` | All WebSocket v2 request/response types, `SubscribeChannel`, `MessageKind`, `identify_message()` |

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

### WebSocket authentication

Private WebSocket channels use a **session token** (not the API key/secret directly):

1. Call `GetWebSocketsTokenRequest` via the REST client to obtain a token.
2. Pass the token as the `"token"` field inside each WebSocket request's `params`.

`WsCredentials` wraps the token; `AddOrderRequest`, `SubscribeRequest`, etc. accept it directly.

### Generic response envelope

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

### WebSocket message dispatch

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

## Adding a new WebSocket message type

1. Add request struct with `json to_json() const` and response struct with `static T from_json(const json&)` in `kraken_ws_api.hpp`.
2. Add the new `MessageKind` enum value.
3. Handle the new method name or channel string in `identify_message()`.

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
- The static library (`libkrakenapi.a`) links against libcurl and OpenSSL. Callers that use the WebSocket layer must separately link against `ixwebsocket`.
- IXWebSocket and spdlog are **not** linked into `libkrakenapi.a`; they are used only by examples and tests.

---

## WebSocket endpoints

| Endpoint | URL |
|---|---|
| Public | `wss://ws.kraken.com/v2` |
| Private (authenticated) | `wss://ws-auth.kraken.com/v2` |

The private endpoint requires a token obtained via `GetWebSocketsTokenRequest` before connecting.

---

## Running examples

```bash
# Public (no credentials needed)
./build/bin/public_rest
./build/bin/public_ws BTC/EUR

# Private (requires ~/.kraken/default)
./build/bin/private_rest
./build/bin/private_ws

# Combined REST + WS demo
./build/bin/kraken_example
```

Callers that embed the REST client must call `curl_global_init(CURL_GLOBAL_ALL)` before constructing `KrakenRestClient` and `curl_global_cleanup()` on teardown.

---

## Testing without network (mock performer)

Unit tests inject a custom HTTP performer via `make_test_client`:

```cpp
auto client = make_test_client([](const kraken::rest::HttpRequest& http) -> std::string {
    // inspect http.path, http.method, http.body, http.headers
    return R"({"error":[],"result":{...}})";
});
auto resp = client.execute(SomeRequest{});
```

This factory is declared `inline` in `kraken_rest_client.hpp` and friends `KrakenRestClient`'s private constructor, so no changes to the library source are needed.
