#-------------------------------------------------------------------------------
# Common scaffold — generic, exchange-agnostic ExchangeWsClient implementation.
#
# This is the one piece of non-template WS-client code shared across every
# exchange adapter. It is a genuine extraction from krakenapi (Step 9): both
# exchange libraries link it PUBLIC, so a Binance-only build never references
# the Kraken library. It needs nlohmann_json transitively, but NOT OpenSSL or
# libcurl — the generic dispatch loop does no crypto or HTTP itself.
#-------------------------------------------------------------------------------

add_library(exchange_common STATIC
    exchange/common/ws_client.cpp
    exchange/common/tick_price.cpp
)
add_library(krakenapi::common ALIAS exchange_common)
target_include_directories(exchange_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(exchange_common PUBLIC nlohmann_json::nlohmann_json)

#-------------------------------------------------------------------------------
# Common HTTP transport — generic libcurl-backed CurlHttpClient (plan 010).
#
# Kept separate from exchange_common so the latter stays curl/ssl-free (the WS
# engine needs no HTTP). Both REST adapters link this; a future exchange reuses
# it. It owns the one piece of shared curl code the per-adapter rest_client.cpp
# files used to duplicate.
#-------------------------------------------------------------------------------

add_library(exchange_http STATIC
    exchange/common/http_client.cpp
)
add_library(krakenapi::http ALIAS exchange_http)
target_include_directories(exchange_http PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(exchange_http PUBLIC CURL::libcurl nlohmann_json::nlohmann_json)

#-------------------------------------------------------------------------------
# Kraken exchange adapter
#-------------------------------------------------------------------------------

if(KRAKENAPI_BUILD_KRAKEN)
    add_library(krakenapi STATIC
        kraken/rest_client.cpp
    )
    add_library(krakenapi::krakenapi ALIAS krakenapi)
    # OpenSSL + libcurl are PUBLIC, not PRIVATE: the public headers auth.hpp
    # (inline HMAC/SHA over libcrypto) and rest_client.hpp (#include <curl/curl.h>)
    # expose these to every consumer's own translation units.
    target_link_libraries(krakenapi
        PUBLIC exchange_common OpenSSL::SSL OpenSSL::Crypto CURL::libcurl
    )
endif()

#-------------------------------------------------------------------------------
# Binance exchange adapter (Step 4+)
#-------------------------------------------------------------------------------

if(KRAKENAPI_BUILD_BINANCE)
    add_library(binanceapi STATIC
        binance/rest_client.cpp
        # No binance/ws_client.cpp: the Binance WS streams + API are header-only
        # (BinanceStreamClient / BinanceWsApiClient are bare aliases for
        # ExchangeWsClient; their factories are inline). The generic impl comes
        # from exchange_common, exactly as it does for Kraken.
    )
    add_library(krakenapi::binanceapi ALIAS binanceapi)
    # exchange_http supplies the shared CurlHttpClient (and libcurl, PUBLIC).
    # OpenSSL stays PUBLIC because auth.hpp's inline HMAC/SHA is exposed in a
    # public header.
    target_link_libraries(binanceapi
        PUBLIC exchange_common exchange_http OpenSSL::SSL OpenSSL::Crypto
    )
endif()
