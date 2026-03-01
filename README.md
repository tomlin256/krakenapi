# krakenapi

A type-safe C++ library for the [Kraken](https://kraken.com) REST and WebSocket v2 APIs.

## Features

- Type-safe REST request/response pairs — compiler links each request to its result type
- Full public and private REST API coverage (market data, account, trading, funding)
- WebSocket v2 support — typed subscription messages and order management
- HMAC-SHA512 signing for authenticated endpoints
- File-based credential loading from `~/.kraken/<name>`
- Unit-tested with Google Test

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
- `build/bin/public_ws` — public WebSocket example
- `build/bin/private_ws` — private WebSocket example
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
│   ├── kraken_types.hpp         # Shared enums and structs (OrderParams, Side, OrderType, …)
│   ├── kraken_rest_api.hpp      # All REST request/response types + signing
│   ├── kraken_rest_client.hpp   # KrakenRestClient — typed HTTP executor
│   └── kraken_ws_api.hpp        # WebSocket v2 request/response types
├── src/
│   ├── CMakeLists.txt
│   └── kraken_rest_client.cpp
└── tests/
    ├── CMakeLists.txt
    ├── examples/
    │   ├── public_rest.cpp
    │   ├── private_rest.cpp
    │   ├── public_ws.cpp
    │   ├── private_ws.cpp
    │   ├── kraken_example.cpp   # REST + WebSocket in one program
    │   ├── kapi.hpp             # Legacy KAPI wrapper (kept for reference)
    │   └── kapi.cpp
    └── unit/
        ├── test_signature.cpp
        ├── test_rest_requests.cpp
        ├── test_rest_responses.cpp
        └── test_client.cpp
```

## API Overview

All library types live in the `kraken::` namespace. REST-specific types are in `kraken::rest::`, WebSocket types in `kraken::ws::`.

### Headers

```cpp
#include "kraken_rest_client.hpp"  // pulls in kraken_rest_api.hpp
#include "kraken_ws_api.hpp"       // WebSocket types
#include "kraken_types.hpp"        // shared types (included by both)
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
| `GetOpenPositionsRequest` | `OpenPositionsResult` |
| `GetLedgersRequest` | `LedgersResult` |

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

### WebSocket v2

The public endpoint is `wss://ws.kraken.com/v2`. The private (authenticated) endpoint is `wss://ws-auth.kraken.com/v2` and requires a token obtained from `GetWebSocketsTokenRequest`.

**Subscriptions**

```cpp
// Ticker
kraken::ws::SubscribeRequest sub;
sub.channel = kraken::ws::SubscribeChannel::Ticker;
sub.symbols = {"BTC/USD", "ETH/USD"};
webSocket.send(sub.to_json().dump());

// Order book (depth 10)
sub.channel = kraken::ws::SubscribeChannel::Book;
sub.symbols = {"BTC/USD"};
sub.depth   = 10;

// OHLC (60-second candles)
sub.channel  = kraken::ws::SubscribeChannel::OHLC;
sub.interval = 60;

// Private: executions (with snapshot)
sub.channel         = kraken::ws::SubscribeChannel::Executions;
sub.snapshot        = true;
sub.snapshot_trades = true;

// Private: balances
sub.channel = kraken::ws::SubscribeChannel::Balances;
```

**Parsing incoming messages**

```cpp
auto json = nlohmann::json::parse(msg->str);
switch (kraken::ws::identify_message(json)) {
    case kraken::ws::MessageKind::Ticker: {
        auto m = kraken::ws::TickerMessage::from_json(json);
        std::cout << m.data[0].symbol << " last=" << m.data[0].last << "\n";
        break;
    }
    case kraken::ws::MessageKind::Executions: {
        auto m = kraken::ws::ExecutionsMessage::from_json(json);
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
| `public_ws` | [public_ws.cpp](tests/examples/public_ws.cpp) | Subscribe to ticker channel for 10 s |
| `private_ws` | [private_ws.cpp](tests/examples/private_ws.cpp) | Subscribe to balances channel for 10 s |
| `kraken_example` | [kraken_example.cpp](tests/examples/kraken_example.cpp) | REST + WebSocket all in one |

```bash
./build/bin/public_rest
./build/bin/private_rest          # requires ~/.kraken/default
./build/bin/public_ws BTC/EUR
./build/bin/private_ws            # requires ~/.kraken/default
```

## Unit Tests

```bash
cd build && ctest --output-on-failure
```

Tests cover:

- **test_signature** — HMAC-SHA512 signing output
- **test_rest_requests** — HTTP request building (paths, query strings, headers)
- **test_rest_responses** — JSON deserialization for all response types
- **test_client** — full request → response cycle with a mock HTTP performer

## License

MIT
