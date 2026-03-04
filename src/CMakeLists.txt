add_library(krakenapi STATIC kraken_rest_client.cpp kraken_ws_client.cpp)
add_library(krakenapi::krakenapi ALIAS krakenapi)
target_include_directories(krakenapi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(krakenapi CURL::libcurl OpenSSL::SSL nlohmann_json::nlohmann_json)
