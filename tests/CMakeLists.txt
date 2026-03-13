include(FetchContent)

#-------------------------------------------------------------------------------
# Find spdlog
#-------------------------------------------------------------------------------

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.17.0
)
FetchContent_MakeAvailable(spdlog)

#-------------------------------------------------------------------------------
# CLI11
#-------------------------------------------------------------------------------

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(CLI11)

#-------------------------------------------------------------------------------
# Legacy KAPI library (lives in examples/, not installed/exported)
#-------------------------------------------------------------------------------

add_library(kapi STATIC examples/kapi.cpp)
target_include_directories(kapi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/examples)
target_link_libraries(kapi CURL::libcurl OpenSSL::SSL)

#-------------------------------------------------------------------------------
# Example apps
#-------------------------------------------------------------------------------

add_executable(private_rest examples/private_rest.cpp)
target_link_libraries(private_rest krakenapi spdlog::spdlog CLI11::CLI11)

add_executable (public_rest examples/public_rest.cpp)
target_link_libraries(public_rest krakenapi spdlog::spdlog CLI11::CLI11)

add_executable (public_ws examples/public_ws.cpp)
target_link_libraries(public_ws krakenapi ixwebsocket spdlog::spdlog CLI11::CLI11)

add_executable (private_ws examples/private_ws.cpp)
target_link_libraries(private_ws krakenapi ixwebsocket spdlog::spdlog CLI11::CLI11)

add_executable (kraken_example examples/kraken_example.cpp)
target_link_libraries(kraken_example krakenapi ixwebsocket spdlog::spdlog CLI11::CLI11)

add_executable (ws_client_example examples/ws_client_example.cpp)
target_link_libraries(ws_client_example krakenapi ixwebsocket spdlog::spdlog CLI11::CLI11)

add_executable (rest_client_example examples/rest_client_example.cpp)
target_link_libraries(rest_client_example krakenapi spdlog::spdlog CLI11::CLI11)

#-------------------------------------------------------------------------------
# Unit tests
#-------------------------------------------------------------------------------

add_subdirectory(unit)