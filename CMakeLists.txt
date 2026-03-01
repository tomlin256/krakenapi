cmake_minimum_required(VERSION 3.15)
project(KRAKENAPI)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#-------------------------------------------------------------------------------
# Library options
#-------------------------------------------------------------------------------

option(KRAKENAPI_BUILD_TESTS "Build tests" ON)

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