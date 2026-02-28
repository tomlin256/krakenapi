# krakenapi

A C++ library for interfacing with the [Kraken](https://kraken.com) REST and WebSocket APIs.

## Features

- Public and private REST API calls
- WebSocket support for real-time market data (public) and account updates (private)
- HMAC-SHA512 request signing for authenticated endpoints
- File-based API key management

## Dependencies

### System (must be installed)

- **OpenSSL** — cryptographic operations
- **libcurl** — HTTP client
- **nlohmann/json** — JSON parsing

On Debian/Ubuntu:
```bash
sudo apt install libssl-dev libcurl4-openssl-dev nlohmann-json3-dev
```

### Fetched automatically by CMake

- [spdlog](https://github.com/gabime/spdlog) v1.13.0 — logging
- [IXWebSocket](https://github.com/machinezone/IXWebSocket) v11.4.6 — WebSocket client

## Building

Requires CMake 3.15+ and a C++17 compiler.

```bash
cmake -B build
cmake --build build
```

The default build type is `Debug`. For a release build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

This produces:
- `build/src/libkrakenapi.a` — static library
- `build/tests/examples/public_method` — public REST API example
- `build/tests/examples/private_method` — private REST API example
- `build/tests/examples/public_ws` — public WebSocket example
- `build/tests/examples/private_ws` — private WebSocket example

## API Overview

All classes and functions live in the `Kraken` namespace.

### Initialization

`KAPI` uses libcurl internally. Call these once at program startup/shutdown:

```cpp
Kraken::initialize(); // wraps curl_global_init
// ...
Kraken::terminate();  // wraps curl_global_cleanup
```

### KAPI class

```cpp
// No credentials (public methods only)
Kraken::KAPI kapi;

// With credentials
Kraken::KAPI kapi(api_key, api_secret);

// Full control over base URL and API version
Kraken::KAPI kapi(api_key, api_secret, url, version);
```

#### Public methods

```cpp
KAPI::Input params;
params["pair"] = "XXBTZEUR";
std::string response = kapi.public_method("Trades", params);

// No parameters
std::string response = kapi.public_method("AssetPairs");
```

#### Private methods

```cpp
KAPI::Input params;
std::string response = kapi.private_method("GetWebSocketsToken", params);

// No parameters
std::string response = kapi.private_method("Balance");
```

All methods return the raw JSON response as a `std::string`.

### Key management

Store credentials in a file with two lines:

```
<api_key>
<private_key>
```

Load them with:

```cpp
// Looks for ~/.kraken/<name>
Kraken::Keys keys = Kraken::load_keys("default");
Kraken::KAPI kapi(keys.apiKey, keys.privateKey);
```

## Examples

### Public REST API

```cpp
#include "kapi.hpp"

int main() {
    curl_global_init(CURL_GLOBAL_ALL);

    Kraken::KAPI kapi;
    Kraken::KAPI::Input in;
    in["pair"] = "XXBTZEUR";
    std::cout << kapi.public_method("Trades", in) << std::endl;

    curl_global_cleanup();
}
```

### Private REST API

```cpp
#include "kapi.hpp"

int main(int argc, char* argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);

    Kraken::KAPI kapi(argv[1], argv[2]);
    std::cout << kapi.private_method("GetWebSocketsToken") << std::endl;

    curl_global_cleanup();
}
```

### Public WebSocket (ticker)

See [tests/examples/public_ws.cpp](tests/examples/public_ws.cpp).

```bash
./build/tests/examples/public_ws BTC/EUR
```

Subscribes to the `ticker` channel and prints bid/ask updates for 10 seconds.

### Private WebSocket (balances)

See [tests/examples/private_ws.cpp](tests/examples/private_ws.cpp).

Loads credentials from `~/.kraken/default`, obtains a WebSocket token via the REST API, then subscribes to the `balances` channel for 10 seconds.

```bash
./build/tests/examples/private_ws
```

## Project Structure

```
krakenapi/
├── CMakeLists.txt
├── include/
│   └── kapi.hpp          # KAPI class, Keys struct, helper declarations
├── src/
│   ├── CMakeLists.txt
│   └── kapi.cpp          # Implementation
└── tests/
    ├── CMakeLists.txt
    └── examples/
        ├── public_method.cpp
        ├── private_method.cpp
        ├── public_ws.cpp
        └── private_ws.cpp
```

## License

MIT
