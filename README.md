# krakenapi

A type-safe C++ library for the [Kraken](https://kraken.com) REST and WebSocket v2 APIs.

## Features

- Type-safe REST request/response pairs — compiler links each request to its result type
- Full public and private REST API coverage (market data, account, trading, funding)
- WebSocket v2 support — typed subscription messages and order management via `KrakenWsClient`
- HMAC-SHA512 signing for authenticated endpoints
- File-based credential loading from `~/.kraken/<name>`
- Unit-tested with Google Test (no network required)

## Dependencies

### System (must be installed)

- **OpenSSL** — HMAC-SHA512 signing
- **libcurl** — HTTP transport

On Debian/Ubuntu:

```bash
sudo apt install libssl-dev libcurl4-openssl-dev
```

### Fetched automatically by CMake

| Library | Version | Purpose |
|---|---|---|
| [IXWebSocket](https://github.com/machinezone/IXWebSocket) | v11.4.6 | WebSocket client (TLS) |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.12.0 | JSON parsing |
| [spdlog](https://github.com/gabime/spdlog) | v1.17.0 | Logging (examples/tests) |
| [Google Test](https://github.com/google/googletest) | v1.16.0 | Unit testing |

## Building

Requires CMake 3.15+ and a C++17 compiler.

```bash
cmake -B build
cmake --build build
```

For a release build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Build outputs:

- `build/src/libkrakenapi.a` — static library
- `build/bin/public_rest` — public REST example
- `build/bin/private_rest` — private REST example
- `build/bin/public_ws` — public WebSocket example (low-level)
- `build/bin/private_ws` — private WebSocket example
- `build/bin/ws_client_example` — `KrakenWsClient` subscription + connection reuse demo
- `build/bin/kraken_example` — comprehensive REST + WebSocket example

To skip building tests and examples:

```bash
cmake -B build -DKRAKENAPI_BUILD_TESTS=OFF
```

## Project Structure

```
krakenapi/
├── CMakeLists.txt
├── include/
│   ├── kraken_types.hpp             # Shared enums and structs (OrderParams, Side, OrderType, …)
│   ├── kraken_rest_api.hpp          # All REST request/response types + signing
│   ├── kraken_rest_client.hpp       # KrakenRestClient — typed HTTP executor
│   ├── kraken_ws_api.hpp            # WebSocket v2 request/response types
│   ├── kraken_ws_client.hpp         # KrakenWsClient + IWsConnection interface
│   ├── kraken_ws_client.inl         # Template method implementations (included by .hpp)
│   └── kraken_ix_ws_connection.hpp  # IxWsConnection (ixwebsocket) + URL factory overload
├── src/
│   ├── CMakeLists.txt
│   ├── kraken_rest_client.cpp
│   └── kraken_ws_client.cpp
└── tests/
    ├── CMakeLists.txt
    ├── examples/
    │   ├── public_rest.cpp
    │   ├── private_rest.cpp
    │   ├── public_ws.cpp
    │   ├── private_ws.cpp
    │   ├── ws_client_example.cpp    # KrakenWsClient subscription + connection reuse
    │   ├── kraken_example.cpp       # REST + WebSocket in one program
    │   ├── kapi.hpp                 # Legacy KAPI wrapper (kept for reference)
    │   └── kapi.cpp
    └── unit/
        ├── test_signature.cpp
        ├── test_rest_requests.cpp
        ├── test_rest_responses.cpp
        ├── test_client.cpp
        └── test_ws_client.cpp
```

## API Overview

All library types live in the `kraken::` namespace. REST-specific types are in `kraken::rest::`, WebSocket types in `kraken::ws::`.

### Headers

```cpp
#include "kraken_rest_client.hpp"      // pulls in kraken_rest_api.hpp
#include "kraken_ix_ws_connection.hpp" // KrakenWsClient + IxWebSocket transport
#include "kraken_types.hpp"            // shared types (included by both)
```

### Credentials

```cpp
// Inline
kraken::rest::Credentials creds{ .api_key = "...", .api_secret = "..." };

// From ~/.kraken/<name>
auto creds = kraken::rest::Credentials::from_file("default");
```

The credentials file must contain the API key on the first line and the base64-encoded private key on the second line.

### REST client

```cpp
curl_global_init(CURL_GLOBAL_ALL);

kraken::rest::KrakenRestClient client;

// Public request
kraken::rest::GetRecentTradesRequest req;
req.pair = "XXBTZEUR";
auto resp = client.execute(req);

// Private request
auto resp2 = client.execute(kraken::rest::GetAccountBalanceRequest{}, creds);

curl_global_cleanup();
```

Every `execute()` call returns a `kraken::RestResponse<T>`:

```cpp
if (resp.ok && resp.result) {
    // use resp.result->...
} else {
    for (const auto& e : resp.errors)
        spdlog::error("{}", e);
}
```

### Public REST endpoints

| Request | Result type |
|---|---|
| `GetServerTimeRequest` | `ServerTime` |
| `GetSystemStatusRequest` | `SystemStatus` |
| `GetAssetInfoRequest` | `AssetInfoResult` |
| `GetAssetPairsRequest` | `AssetPairsResult` |
| `GetTickerRequest` | `TickerResult` |
| `GetOHLCRequest` | `OHLCResult` |
| `GetOrderBookRequest` | `OrderBookResult` |
| `GetRecentTradesRequest` | `RecentTradesResult` |

### Private REST endpoints

**Account data**

| Request | Result type |
|---|---|
| `GetAccountBalanceRequest` | `AccountBalanceResult` |
| `GetExtendedBalanceRequest` | `ExtendedBalanceResult` |
| `GetTradeBalanceRequest` | `TradeBalance` |
| `GetOpenOrdersRequest` | `OpenOrdersResult` |
| `GetClosedOrdersRequest` | `ClosedOrdersResult` |
| `QueryOrdersRequest` | `QueryOrdersResultWrapper` |
| `GetTradesHistoryRequest` | `TradesHistoryResult` |
| `QueryTradesRequest` | `QueryTradesResultWrapper` |
| `GetOpenPositionsRequest` | `OpenPositionsResult` |
| `GetLedgersRequest` | `LedgersResult` |
| `QueryLedgersRequest` | `QueryLedgersResultWrapper` |

**Trading**

| Request | Result type |
|---|---|
| `AddOrderRequest` | `AddOrderResult` |
| `AddOrderBatchRequest` | `AddOrderBatchResult` |
| `EditOrderRequest` | `EditOrderResult` |
| `AmendOrderRequest` | `AmendOrderResult` |
| `CancelOrderRequest` | `CancelOrderResult` |
| `CancelAllOrdersRequest` | `CancelAllResult` |
| `CancelAllOrdersAfterRequest` | `CancelAllAfterResult` |
| `CancelOrderBatchRequest` | `CancelOrderBatchResult` |

**Other**

| Request | Result type |
|---|---|
| `GetWebSocketsTokenRequest` | `WebSocketsTokenResult` |
| `GetDepositMethodsRequest` | `DepositMethodsResult` |
| `GetDepositAddressesRequest` | `DepositAddressesResult` |
| `WithdrawRequest` | `WithdrawResult` |
| `CancelWithdrawalRequest` | `CancelWithdrawalResult` |
| `CreateSubaccountRequest` | `CreateSubaccountResult` |
| `AllocateEarnRequest` | `EarnBoolResult` |
| `DeallocateEarnRequest` | `EarnBoolResult` |

### Placing an order

`OrderParams` is the shared order description used by both the REST and WebSocket layers:

```cpp
kraken::OrderParams op;
op.order_type  = kraken::OrderType::Limit;
op.side        = kraken::Side::Buy;
op.symbol      = "XBTUSD";
op.order_qty   = 1.2;
op.limit_price = 26500.0;

// REST
kraken::rest::AddOrderRequest req;
req.params = op;
auto resp = client.execute(req, creds);
if (resp.ok)
    std::cout << resp.result->txids[0] << "\n";

// WebSocket (change symbol to WS format first)
op.symbol = "BTC/USD";
kraken::ws::AddOrderRequest ws_req;
ws_req.order_type = op.order_type;
// send ws_req.to_json() over the authenticated WebSocket
```

### WebSocket v2 — KrakenWsClient

`KrakenWsClient` provides a type-safe wrapper over the raw WebSocket connection with two modes of operation.

**Connecting**

```cpp
#include "kraken_ix_ws_connection.hpp"

auto client = kraken::ws::make_ws_client(kraken::ws::PUBLIC_WS_URL);
```

**Method calls** (single request → single response):

```cpp
// Blocking
auto resp = client->execute(kraken::ws::PingRequest{});  // WsResponse<PongMessage>
if (resp.ok) { /* success */ }

// Non-blocking
auto fut = client->execute_async(kraken::ws::AddOrderRequest{...});
auto resp = fut.get();
```

**Subscriptions** (request → ack + continuous push callbacks):

```cpp
kraken::ws::TickerSubscribeRequest sub;
sub.channel = kraken::ws::SubscribeChannel::Ticker;
sub.symbols = {"BTC/USD", "ETH/USD"};

auto [ack, handle] = client->subscribe(
    sub,
    [](kraken::ws::TickerMessage msg) {
        std::cout << msg.data[0].symbol << " last=" << msg.data[0].last << "\n";
    }
);

if (!ack.ok) { /* handle error */ }

// Later:
handle.cancel();  // unsubscribe; idempotent
```

**Subscription channels**

| Type alias | Channel | Push message | Auth required |
|---|---|---|---|
| `TickerSubscribeRequest` | `Ticker` | `TickerMessage` | No |
| `BookSubscribeRequest` | `Book` | `BookMessage` | No |
| `TradeSubscribeRequest` | `Trade` | `TradeMessage` | No |
| `OHLCSubscribeRequest` | `OHLC` | `OHLCMessage` | No |
| `InstrumentSubscribeRequest` | `Instrument` | `InstrumentMessage` | No |
| `ExecutionsSubscribeRequest` | `Executions` | `ExecutionsMessage` | Yes |
| `BalancesSubscribeRequest` | `Balances` | `BalancesMessage` | Yes |

**Low-level message parsing** (when bypassing `KrakenWsClient`):

```cpp
auto j = nlohmann::json::parse(raw);
switch (kraken::ws::identify_message(j)) {
    case kraken::ws::MessageKind::Ticker: {
        auto m = kraken::ws::TickerMessage::from_json(j);
        std::cout << m.data[0].symbol << " last=" << m.data[0].last << "\n";
        break;
    }
    case kraken::ws::MessageKind::Executions: {
        auto m = kraken::ws::ExecutionsMessage::from_json(j);
        std::cout << "order=" << m.data[0].order_id << "\n";
        break;
    }
    default: break;
}
```

## Examples

See [tests/examples/](tests/examples/) for complete, buildable programs.

| Binary | Source | What it does |
|---|---|---|
| `public_rest` | [public_rest.cpp](tests/examples/public_rest.cpp) | Fetch recent trades for XXBTZEUR |
| `private_rest` | [private_rest.cpp](tests/examples/private_rest.cpp) | Get a WebSocket token using credentials |
| `public_ws` | [public_ws.cpp](tests/examples/public_ws.cpp) | Subscribe to ticker channel (low-level) |
| `private_ws` | [private_ws.cpp](tests/examples/private_ws.cpp) | Subscribe to balances channel |
| `ws_client_example` | [ws_client_example.cpp](tests/examples/ws_client_example.cpp) | `KrakenWsClient` subscription + connection reuse |
| `kraken_example` | [kraken_example.cpp](tests/examples/kraken_example.cpp) | REST + WebSocket all in one |

```bash
./build/bin/public_rest
./build/bin/private_rest          # requires ~/.kraken/default
./build/bin/public_ws BTC/EUR
./build/bin/private_ws            # requires ~/.kraken/default
./build/bin/ws_client_example BTC/USD
```

## Unit Tests

```bash
cd build && ctest --output-on-failure
```

Tests cover:

- **test_signature** — HMAC-SHA512 signing output matches reference implementation
- **test_rest_requests** — HTTP request building (paths, query strings, headers)
- **test_rest_responses** — JSON deserialization for all response types
- **test_client** — full request → response cycle with a mock HTTP performer
- **test_ws_client** — `KrakenWsClient` lifecycle with `MockWsConnection` (no network)

## License

MIT — Copyright (c) 2026 Rob Tomlin. See [LICENSE](LICENSE).
