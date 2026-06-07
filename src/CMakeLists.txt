add_library(krakenapi STATIC
    # Common scaffold — generic exchange::ws:: implementation shared across exchanges
    exchange/common/ws_client.cpp
    # Old kraken:: implementations — retired in Phase 2 of plan 002
    kraken_types.cpp
    kraken_rest_client.cpp
    # New exchange::kraken:: namespace implementations
    kraken/types.cpp
    kraken/rest_client.cpp
)
add_library(krakenapi::krakenapi ALIAS krakenapi)
target_include_directories(krakenapi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(krakenapi CURL::libcurl OpenSSL::SSL nlohmann_json::nlohmann_json)
