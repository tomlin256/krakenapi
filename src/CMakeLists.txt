add_library(krakenapi STATIC
    # Common scaffold — generic exchange::ws:: implementation shared across exchanges
    exchange/common/ws_client.cpp
    # exchange::kraken:: namespace implementations
    kraken/types.cpp
    kraken/rest_client.cpp
)
add_library(krakenapi::krakenapi ALIAS krakenapi)
target_include_directories(krakenapi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(krakenapi CURL::libcurl OpenSSL::SSL nlohmann_json::nlohmann_json)

#-------------------------------------------------------------------------------
# Binance exchange adapter (Step 4+)
#-------------------------------------------------------------------------------

add_library(binanceapi STATIC
    binance/rest_client.cpp
)
add_library(krakenapi::binanceapi ALIAS binanceapi)
target_include_directories(binanceapi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(binanceapi CURL::libcurl OpenSSL::SSL nlohmann_json::nlohmann_json)
