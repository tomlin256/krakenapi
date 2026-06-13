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
)
add_library(krakenapi::common ALIAS exchange_common)
target_include_directories(exchange_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(exchange_common PUBLIC nlohmann_json::nlohmann_json)

#-------------------------------------------------------------------------------
# Kraken exchange adapter
#-------------------------------------------------------------------------------

if(KRAKENAPI_BUILD_KRAKEN)
    add_library(krakenapi STATIC
        kraken/types.cpp
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
    # PUBLIC for the same reason as krakenapi — see the note above.
    target_link_libraries(binanceapi
        PUBLIC exchange_common OpenSSL::SSL OpenSSL::Crypto CURL::libcurl
    )
endif()
