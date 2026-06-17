#-------------------------------------------------------------------------------
# Common scaffold — generic, exchange-agnostic ExchangeWsClient implementation.
#
# This is the one piece of non-template WS-client code shared across every
# exchange adapter. It is a genuine extraction from cryptocogs (Step 9): both
# exchange libraries link it PUBLIC, so a Binance-only build never references
# the Kraken library. It needs nlohmann_json transitively, but NOT OpenSSL or
# libcurl — the generic dispatch loop does no crypto or HTTP itself.
#-------------------------------------------------------------------------------

add_library(exchange_common STATIC
    exchange/common/ws_client.cpp
    exchange/common/ws.cpp
    exchange/common/tick_price.cpp
    exchange/common/types.cpp
)
add_library(cryptocogs::common ALIAS exchange_common)
target_include_directories(exchange_common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
# nlohmann_json is header-only and FetchContent'd (a non-installable local
# target), so it can't go in our install export. We link it for the BUILD only
# and vendor its headers into our install prefix (see the top-level CMakeLists),
# making the installed package self-contained — consumers need no separate
# nlohmann_json. The INSTALL_INTERFACE include dir already covers the vendored
# copy.
target_link_libraries(exchange_common PUBLIC
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)

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
add_library(cryptocogs::http ALIAS exchange_http)
target_include_directories(exchange_http PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(exchange_http PUBLIC
    CURL::libcurl
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)

#-------------------------------------------------------------------------------
# Kraken exchange adapter
#-------------------------------------------------------------------------------

if(CRYPTOCOGS_BUILD_KRAKEN)
    add_library(kraken STATIC
        kraken/types.cpp
        kraken/auth.cpp
        kraken/rest_api.cpp
        kraken/rest_client.cpp
        kraken/ws_api.cpp
        kraken/ws_client.cpp
    )
    add_library(cryptocogs::kraken ALIAS kraken)
    # exchange_http supplies the shared CurlHttpClient (and libcurl, PUBLIC).
    # OpenSSL stays PUBLIC because auth.hpp's inline HMAC/SHA is exposed in a
    # public header.
    target_link_libraries(kraken
        PUBLIC exchange_common exchange_http OpenSSL::SSL OpenSSL::Crypto
    )
endif()

#-------------------------------------------------------------------------------
# Binance exchange adapter (Step 4+)
#-------------------------------------------------------------------------------

if(CRYPTOCOGS_BUILD_BINANCE)
    add_library(binance STATIC
        binance/rest_client.cpp
        # No binance/ws_client.cpp: the Binance WS streams + API are header-only
        # (BinanceStreamClient / BinanceWsApiClient are bare aliases for
        # ExchangeWsClient; their factories are inline). The generic impl comes
        # from exchange_common, exactly as it does for Kraken.
    )
    add_library(cryptocogs::binance ALIAS binance)
    # exchange_http supplies the shared CurlHttpClient (and libcurl, PUBLIC).
    # OpenSSL stays PUBLIC because auth.hpp's inline HMAC/SHA is exposed in a
    # public header.
    target_link_libraries(binance
        PUBLIC exchange_common exchange_http OpenSSL::SSL OpenSSL::Crypto
    )
endif()

#-------------------------------------------------------------------------------
# Install the library targets into the cryptocogsTargets export set (plan 012).
# The export itself + headers + package config are installed from the top-level
# CMakeLists. The two common libs are always present; the adapters only if their
# flag built them.
#-------------------------------------------------------------------------------

if(CRYPTOCOGS_INSTALL)
    set(_cryptocogs_targets exchange_common exchange_http)
    if(TARGET kraken)
        list(APPEND _cryptocogs_targets kraken)
    endif()
    if(TARGET binance)
        list(APPEND _cryptocogs_targets binance)
    endif()
    install(TARGETS ${_cryptocogs_targets}
            EXPORT      cryptocogsTargets
            ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
