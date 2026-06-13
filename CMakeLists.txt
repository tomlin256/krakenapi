cmake_minimum_required(VERSION 3.15...4.2)
project(KRAKENAPI VERSION 0.1.0 LANGUAGES CXX)

# Allow fetched dependencies that declare cmake_minimum_required < 3.5
# (CMake 4.x removed compatibility below 3.5)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)
include(GNUInstallDirs)

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

# Generate install + package-config rules by default for a top-level build;
# off when krakenapi is pulled in as a subproject (e.g. via FetchContent), so it
# never pollutes a parent project's install.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_kapi_install_default ON)
else()
    set(_kapi_install_default OFF)
endif()
option(KRAKENAPI_INSTALL "Generate install rules and a CMake package config" ${_kapi_install_default})

#-------------------------------------------------------------------------------
# Find OpenSSL
#-------------------------------------------------------------------------------

find_package(OpenSSL REQUIRED)

#-------------------------------------------------------------------------------
# Find CURL
#-------------------------------------------------------------------------------

find_package(CURL REQUIRED)

#-------------------------------------------------------------------------------
# A sensible json library
#
# JSON_Install OFF: nlohmann ships its own install rules; we don't want them
# polluting a krakenapi install — its headers are vendored explicitly from the
# install block below.
#-------------------------------------------------------------------------------

set(JSON_Install OFF CACHE BOOL "" FORCE)
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
  # ixwebsocket — used only by examples + tests (no core library links it), so
  # it is fetched here and never installed by krakenapi.
  #-----------------------------------------------------------------------------
  FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG        v11.4.6
  )
  set(USE_TLS ON CACHE BOOL "" FORCE)
  set(USE_OPEN_SSL ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(ixwebsocket)

  #-----------------------------------------------------------------------------
  # Google Test (INSTALL_GTEST OFF — never install gtest/gmock from here)
  #-----------------------------------------------------------------------------
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
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

#-------------------------------------------------------------------------------
# Install + CMake package config (plan 012)
#
# Lets a downstream project `find_package(krakenapi)` and link
# `krakenapi::krakenapi` / `krakenapi::binanceapi` against an installed prefix.
# The library targets are added to the krakenapiTargets export in src/.
#-------------------------------------------------------------------------------

if(KRAKENAPI_INSTALL)
  include(CMakePackageConfigHelpers)

  install(EXPORT krakenapiTargets
          NAMESPACE   krakenapi::
          FILE        krakenapiTargets.cmake
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/krakenapi)

  # Public headers — component-gated to match the build flags.
  install(DIRECTORY include/exchange/common
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exchange)
  if(KRAKENAPI_BUILD_KRAKEN)
    install(DIRECTORY include/exchange/kraken
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exchange)
  endif()
  if(KRAKENAPI_BUILD_BINANCE)
    install(DIRECTORY include/exchange/binance
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exchange)
  endif()
  if(KRAKENAPI_BUILD_COMPAT_SHIM)
    install(FILES
            include/kraken_compat.hpp
            include/kraken_types.hpp
            include/kraken_rest_api.hpp
            include/kraken_rest_client.hpp
            include/kraken_ws_api.hpp
            include/kraken_ws_client.hpp
            include/kraken_ws_client.inl
            include/kraken_ix_ws_connection.hpp
            include/ws_reconnect_session.hpp
            include/ws_reconnect_session.inl
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  endif()

  # Vendor the header-only nlohmann_json into our prefix. It is FetchContent'd
  # (a non-installable local target), so it cannot be an export dependency;
  # shipping its headers here makes the installed package self-contained — the
  # public headers' #include <nlohmann/json.hpp> resolves with no separate
  # nlohmann_json install on the consumer side.
  install(DIRECTORY ${nlohmann_json_SOURCE_DIR}/include/nlohmann
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  configure_package_config_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/krakenapiConfig.cmake.in
      ${CMAKE_CURRENT_BINARY_DIR}/krakenapiConfig.cmake
      INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/krakenapi)
  write_basic_package_version_file(
      ${CMAKE_CURRENT_BINARY_DIR}/krakenapiConfigVersion.cmake
      VERSION       ${PROJECT_VERSION}
      COMPATIBILITY SameMajorVersion)
  install(FILES
          ${CMAKE_CURRENT_BINARY_DIR}/krakenapiConfig.cmake
          ${CMAKE_CURRENT_BINARY_DIR}/krakenapiConfigVersion.cmake
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/krakenapi)
endif()