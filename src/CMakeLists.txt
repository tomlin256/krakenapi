add_library(krakenapi STATIC kapi.cpp)
add_library(krakenapi::krakenapi ALIAS krakenapi)
target_include_directories(krakenapi PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(krakenapi CURL::libcurl OpenSSL::SSL nlohmann_json::nlohmann_json)
