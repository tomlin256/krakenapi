cmake_minimum_required(VERSION 3.15...4.2)
project(KRAKENAPI)

# Allow fetched dependencies that declare cmake_minimum_required < 3.5
# (CMake 4.x removed compatibility below 3.5)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#-------------------------------------------------------------------------------
# Library options
#-------------------------------------------------------------------------------

option(KRAKENAPI_BUILD_KRAKEN  "Build the Kraken exchange adapter"  ON)
option(KRAKENAPI_BUILD_BINANCE "Build the Binance exchange adapter" ON)
option(KRAKENAPI_BUILD_TESTS "Build tests" ON)
option(KRAKENAPI_BUILD_COMPAT_SHIM
       "Install deprecated kraken_*.hpp compatibility headers (source-compat for pre-refactor callers)"
       ON)

# Dependency rules between the exchange/feature flags, checked at configure time.
if(KRAKENAPI_BUILD_COMPAT_SHIM AND NOT KRAKENAPI_BUILD_KRAKEN)
    message(WARNING
        "KRAKENAPI_BUILD_COMPAT_SHIM requires KRAKENAPI_BUILD_KRAKEN; disabling shim.")
    set(KRAKENAPI_BUILD_COMPAT_SHIM OFF CACHE BOOL "" FORCE)
endif()

if(NOT KRAKENAPI_BUILD_KRAKEN AND NOT KRAKENAPI_BUILD_BINANCE)
    message(WARNING
        "Both KRAKENAPI_BUILD_KRAKEN and KRAKENAPI_BUILD_BINANCE are OFF — nothing will be built.")
endif()

#-------------------------------------------------------------------------------
# Find OpenSSL
#-------------------------------------------------------------------------------

find_package(OpenSSL REQUIRED)

#-------------------------------------------------------------------------------
# Find CURL
#-------------------------------------------------------------------------------

find_package(CURL REQUIRED)

#-------------------------------------------------------------------------------
# Add ixwebsocket
#-------------------------------------------------------------------------------

FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v11.4.6
)
set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_OPEN_SSL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

#-------------------------------------------------------------------------------
# A sensible json library
#-------------------------------------------------------------------------------

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

#-------------------------------------------------------------------------------
# Subdirectories
#-------------------------------------------------------------------------------

add_subdirectory(src)

if(KRAKENAPI_BUILD_TESTS)
  #-----------------------------------------------------------------------------
  # Google Test
  #-----------------------------------------------------------------------------
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.16.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_subdirectory(tests)
endif()