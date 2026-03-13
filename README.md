# krakenapi

A type-safe C++ library for the [Kraken](https://kraken.com) Spot REST and WebSocket v2 APIs.

The compiler links each request type to its exact response type — no casts, no stringly-typed keys.

---

## Quick start

```bash
# 1. Install system dependencies (Debian/Ubuntu)
sudo apt install libssl-dev libcurl4-openssl-dev

# 2. Clone and build
git clone https://github.com/tomlin256/krakenapi.git
cd krakenapi
cmake -B build && cmake --build build

# 3. Run an example
./build/bin/rest_client_example time
./build/bin/rest_client_example ticker --pairs XXBTZUSD
./build/bin/ws_client_example ticker BTC/USD
```

All other dependencies (IXWebSocket, nlohmann/json, spdlog, Google Test) are fetched automatically by CMake.

---

## Examples

The fastest way to understand the library is to read and run the examples in [`tests/examples/`](tests/examples/). Each one is a complete, buildable program.

### `rest_client_example` — all public REST endpoints

```bash
./build/bin/rest_client_example time
./build/bin/rest_client_example status
./build/bin/rest_client_example assets   --assets XBT,ETH
./build/bin/rest_client_example pairs    --pairs XBTUSD,ETHUSD
./build/bin/rest_client_example ticker   --pairs XXBTZUSD,XETHZUSD
./build/bin/rest_client_example ohlc     XXBTZUSD --interval 60
./build/bin/rest_client_example depth    XXBTZUSD --count 10
./build/bin/rest_client_example trades   XXBTZUSD --count 5
```

Covers every public REST endpoint in a single binary. Each subcommand maps to one typed request/response pair — no raw JSON, no casts.
Source: [tests/examples/rest_client_example.cpp](tests/examples/rest_client_example.cpp)

```cpp
#include "kraken_rest_client.hpp"

curl_global_init(CURL_GLOBAL_ALL);
kraken::rest::KrakenRestClient client;

// Server time
auto time_resp = client.execute(kraken::rest::GetServerTimeRequest{});
// time_resp is RestResponse<ServerTime>
if (time_resp.ok && time_resp.result)
    spdlog::info("unixtime={} rfc1123={}", time_resp.result->unixtime,
                 time_resp.result->rfc1123);

// OHLC candles
kraken::rest::GetOHLCRequest ohlc_req;
ohlc_req.pair     = "XXBTZUSD";
ohlc_req.interval = 60;  // 1-hour candles

auto ohlc_resp = client.execute(ohlc_req);  // RestResponse<OHLCResult>
if (ohlc_resp.ok && ohlc_resp.result)
    for (const auto& c : ohlc_resp.result->candles)
        spdlog::info("ts={} O={:.4f} H={:.4f} L={:.4f} C={:.4f}",
                     c.time, c.open, c.high, c.low, c.close);

// Order book
kraken::rest::GetOrderBookRequest depth_req;
depth_req.pair  = "XXBTZUSD";
depth_req.count = 10;

auto depth_resp = client.execute(depth_req);  // RestResponse<OrderBookResult>
if (depth_resp.ok && depth_resp.result) {
    for (const auto& ask : depth_resp.result->asks)
        spdlog::info("ask price={:.4f} vol={:.6f}", ask.price, ask.volume);
}

curl_global_cleanup();
```

---

### `public_rest` — fetch recent trades (no credentials)

```bash
./build/bin/public_rest
```

Calls `GET /0/public/Trades` and prints each trade's price, volume, and side.
Source: [tests/examples/public_rest.cpp](tests/examples/public_rest.cpp)

```cpp
#include "kraken_rest_client.hpp"

curl_global_init(CURL_GLOBAL_ALL);
kraken::rest::KrakenRestClient client;

kraken::rest::GetRecentTradesRequest req;
req.pair = "XXBTZEUR";

auto resp = client.execute(req);  // RestResponse<RecentTradesResult>
if (resp.ok && resp.result) {
    for (const auto& t : resp.result->trades)
        spdlog::info("price={} volume={} side={}", t.price, t.volume,
                     t.side == kraken::Side::Buy ? "buy" : "sell");
}
curl_global_cleanup();
```

---

### `private_rest` — authenticated REST call

```bash
./build/bin/private_rest   # requires ~/.kraken/default
```

Loads credentials from `~/.kraken/default` and calls `POST /0/private/GetWebSocketsToken`.
Source: [tests/examples/private_rest.cpp](tests/examples/private_rest.cpp)

```cpp
#include "kraken_rest_client.hpp"

kraken::rest::KrakenRestClient client;
auto creds = kraken::rest::Credentials::from_file("default");

auto resp = client.execute(kraken::rest::GetWebSocketsTokenRequest{}, creds);
if (resp.ok && resp.result)
    spdlog::info("token={} expires={}", resp.result->token, resp.result->expires);
```

---

### `ws_client_example` — typed WebSocket subscriptions via `KrakenWsClient`

```bash
./build/bin/ws_client_example ticker     BTC/USD
./build/bin/ws_client_example book       BTC/USD --depth 10
./build/bin/ws_client_example trade      ETH/USD
./build/bin/ws_client_example ohlc       BTC/USD --interval 5
./build/bin/ws_client_example instrument
```

Uses `KrakenWsClient` — the high-level WebSocket wrapper — to subscribe to any public channel. Demonstrates typed subscribe/unsubscribe and connection reuse.
Source: [tests/examples/ws_client_example.cpp](tests/examples/ws_client_example.cpp)

```cpp
#include "kraken_ix_ws_connection.hpp"  // real ixwebsocket transport

auto client = kraken::ws::make_ws_client(kraken::ws::PUBLIC_WS_URL);

kraken::ws::TickerSubscribeRequest req;
req.symbols = {"BTC/USD"};

auto [ack, handle] = client->subscribe(
    req,
    [](kraken::ws::TickerMessage msg) {
        spdlog::info("{} last={}", msg.data[0].symbol, msg.data[0].last);
    }
);

if (!ack.ok)
    spdlog::error("subscribe failed: {}", ack.error.value_or("unknown"));

std::this_thread::sleep_for(std::chrono::seconds(10));
handle.cancel();  // unsubscribe; safe to call from any thread
```

---

### `public_ws` — low-level WebSocket (raw ixwebsocket)

```bash
./build/bin/public_ws BTC/EUR
```

Builds and sends a `SubscribeRequest` manually using raw ixwebsocket callbacks, then dispatches inbound frames with `identify_message()`. Useful if you already have a WebSocket transport layer.
Source: [tests/examples/public_ws.cpp](tests/examples/public_ws.cpp)

```cpp
#include "kraken_ws_api.hpp"

// On WebSocket open:
kraken::ws::SubscribeRequest req;
req.channel = kraken::ws::SubscribeChannel::Ticker;
req.symbols = {"BTC/USD"};
webSocket.send(req.to_json().dump());

// On each inbound message:
auto j = nlohmann::json::parse(msg->str);
switch (kraken::ws::identify_message(j)) {
    case kraken::ws::MessageKind::Ticker: {
        auto m = kraken::ws::TickerMessage::from_json(j);
        spdlog::info("{} last={}", m.data[0].symbol, m.data[0].last);
        break;
    }
    default: break;
}
```

---

### `private_ws` — authenticated WebSocket (balances stream)

```bash
./build/bin/private_ws   # requires ~/.kraken/default
```

Fetches a WebSocket token via REST, connects to the private endpoint, and subscribes to the `Balances` channel.
Source: [tests/examples/private_ws.cpp](tests/examples/private_ws.cpp)

```cpp
// 1. Get token via REST
auto resp = client.execute(kraken::rest::GetWebSocketsTokenRequest{}, creds);
std::string token = resp.result->token;

// 2. Subscribe to private channel
kraken::ws::SubscribeRequest req;
req.channel = kraken::ws::SubscribeChannel::Balances;
req.token   = token;
webSocket.send(req.to_json().dump());

// 3. Dispatch inbound messages
case kraken::ws::MessageKind::Balances: {
    auto m = kraken::ws::BalancesMessage::from_json(j);
    // m.data contains balance updates per asset
    break;
}
```

---

### `kraken_example` — REST + WebSocket combined demo

Source: [tests/examples/kraken_example.cpp](tests/examples/kraken_example.cpp)

Shows how the REST and WebSocket layers share types from `kraken_types.hpp` — the same `OrderParams`, `Side`, and `OrderType` enums are used for placing orders on both transports.

---

## Building your own project

### CMake (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(krakenapi
    GIT_REPOSITORY https://github.com/tomlin256/krakenapi.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(krakenapi)

target_link_libraries(my_app PRIVATE krakenapi::krakenapi)
```

### Linker flags (manual)

```
-L/path/to/krakenapi/build/src -lkrakenapi -lcurl -lssl -lcrypto
```

For WebSocket support, also link against ixwebsocket:

```
-lixwebsocket -lz
```

---

## Usage patterns

### Credentials

```cpp
// From ~/.kraken/default (line 1: api key, line 2: base64 secret)
auto creds = kraken::rest::Credentials::from_file("default");

// Inline
kraken::rest::Credentials creds{ .api_key = "...", .api_secret = "..." };
```

### REST — public endpoint

```cpp
curl_global_init(CURL_GLOBAL_ALL);
kraken::rest::KrakenRestClient client;

kraken::rest::GetTickerRequest req;
req.pair = "XXBTZUSD";

auto resp = client.execute(req);   // RestResponse<TickerResult>
if (resp.ok && resp.result) {
    // use resp.result->...
}
curl_global_cleanup();
```

### REST — private endpoint

```cpp
auto resp = client.execute(kraken::rest::GetAccountBalanceRequest{}, creds);
// resp is RestResponse<AccountBalanceResult>
```

Always check `resp.ok` before dereferencing `resp.result`:

```cpp
if (resp.ok && resp.result) {
    // safe to use resp.result->...
} else {
    for (const auto& e : resp.errors)
        spdlog::error("{}", e);
}
```

### REST — placing an order

```cpp
kraken::rest::AddOrderRequest req;
req.params.order_type  = kraken::OrderType::Limit;
req.params.side        = kraken::Side::Buy;
req.params.symbol      = "XBTUSD";
req.params.limit_price = 26500.0;
req.params.qty         = 0.001;

auto resp = client.execute(req, creds);  // RestResponse<AddOrderResult>
if (resp.ok && resp.result)
    spdlog::info("txid: {}", resp.result->txids[0]);
```

### WebSocket — `KrakenWsClient` (recommended)

`KrakenWsClient` is the high-level wrapper. It handles connection lifecycle, auto-assigns request IDs, matches responses to pending handlers, and exposes typed callbacks.

```cpp
#include "kraken_ix_ws_connection.hpp"

// Connect
auto client = kraken::ws::make_ws_client(kraken::ws::PUBLIC_WS_URL);

// Ping (single request → single response)
auto pong = client->execute(kraken::ws::PingRequest{});  // WsResponse<PongMessage>

// Subscribe (request → ack + continuous push)
kraken::ws::BookSubscribeRequest sub;
sub.symbols = {"BTC/USD"};
sub.depth   = 10;

auto [ack, handle] = client->subscribe(
    sub,
    [](kraken::ws::BookMessage msg) {
        // called for every book update
    }
);

handle.cancel();  // stop receiving updates
```

### WebSocket — async variants

```cpp
// Fire and forget; returns std::future
auto fut = client->execute_async(kraken::ws::PingRequest{});

auto [ack_fut, handle] = client->subscribe_async(sub, callback);
auto ack = ack_fut.get();
```

### WebSocket — subscription channels

| Channel | Request type | Push message | Auth required |
|---|---|---|---|
| `Ticker` | `TickerSubscribeRequest` | `TickerMessage` | No |
| `Book` | `BookSubscribeRequest` | `BookMessage` | No |
| `Trade` | `TradeSubscribeRequest` | `TradeMessage` | No |
| `OHLC` | `OHLCSubscribeRequest` | `OHLCMessage` | No |
| `Instrument` | `InstrumentSubscribeRequest` | `InstrumentMessage` | No |
| `Level3` | `Level3SubscribeRequest` | `Level3Message` | No |
| `Executions` | `ExecutionsSubscribeRequest` | `ExecutionsMessage` | Yes |
| `Balances` | `BalancesSubscribeRequest` | `BalancesMessage` | Yes |

Private channels need a token obtained via `GetWebSocketsTokenRequest` over REST. Pass it in the subscribe request's `token` field.

---

## REST endpoint reference

### Public

| Request | Path | Response |
|---|---|---|
| `GetServerTimeRequest` | `/0/public/Time` | `ServerTime` |
| `GetSystemStatusRequest` | `/0/public/SystemStatus` | `SystemStatus` |
| `GetAssetInfoRequest` | `/0/public/Assets` | `AssetInfoResult` |
| `GetAssetPairsRequest` | `/0/public/AssetPairs` | `AssetPairsResult` |
| `GetTickerRequest` | `/0/public/Ticker` | `TickerResult` |
| `GetOHLCRequest` | `/0/public/OHLC` | `OHLCResult` |
| `GetOrderBookRequest` | `/0/public/Depth` | `OrderBookResult` |
| `GetRecentTradesRequest` | `/0/public/Trades` | `RecentTradesResult` |

### Private — account

| Request | Response |
|---|---|
| `GetAccountBalanceRequest` | `AccountBalanceResult` |
| `GetExtendedBalanceRequest` | `ExtendedBalanceResult` |
| `GetTradeBalanceRequest` | `TradeBalance` |
| `GetOpenOrdersRequest` | `OpenOrdersResult` |
| `GetClosedOrdersRequest` | `ClosedOrdersResult` |
| `GetTradesHistoryRequest` | `TradesHistoryResult` |
| `GetOpenPositionsRequest` | `OpenPositionsResult` |
| `GetLedgersRequest` | `LedgersResult` |

### Private — trading

| Request | Response |
|---|---|
| `AddOrderRequest` | `AddOrderResult` |
| `AddOrderBatchRequest` | `AddOrderBatchResult` |
| `EditOrderRequest` | `EditOrderResult` |
| `AmendOrderRequest` | `AmendOrderResult` |
| `CancelOrderRequest` | `CancelOrderResult` |
| `CancelAllOrdersRequest` | `CancelAllResult` |
| `CancelAllOrdersAfterRequest` | `CancelAllAfterResult` |
| `CancelOrderBatchRequest` | `CancelOrderBatchResult` |

### Private — other

| Request | Response |
|---|---|
| `GetWebSocketsTokenRequest` | `WebSocketsTokenResult` |
| `GetDepositMethodsRequest` | `DepositMethodsResult` |
| `GetDepositAddressesRequest` | `DepositAddressesResult` |
| `WithdrawRequest` | `WithdrawResult` |
| `CancelWithdrawalRequest` | `CancelWithdrawalResult` |
| `AllocateEarnRequest` | `EarnBoolResult` |
| `DeallocateEarnRequest` | `EarnBoolResult` |

---

## Running the tests

```bash
cd build && ctest --output-on-failure
```

Tests require no network access or credentials — all I/O is mocked.

| Binary | What it covers |
|---|---|
| `kraken_unit_tests` | HMAC signing, REST request building, JSON deserialization, HTTP mock round-trip |
| `test_ws_client` | `KrakenWsClient` lifecycle, subscription handle, pre-connection queue |

---

## Dependencies

| Library | Version | Fetched by CMake |
|---|---|---|
| [IXWebSocket](https://github.com/machinezone/IXWebSocket) | v11.4.6 | Yes |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.12.0 | Yes |
| [spdlog](https://github.com/gabime/spdlog) | v1.17.0 | Yes |
| [Google Test](https://github.com/google/googletest) | v1.16.0 | Yes |
| OpenSSL | system | No — `sudo apt install libssl-dev` |
| libcurl | system | No — `sudo apt install libcurl4-openssl-dev` |

---

## License

MIT
