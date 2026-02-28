cmake_minimum_required (VERSION 3.15)
project (KRAKENAPI)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-------------------------------------------------------------------------------
# Control CMAKE_BUILD_TYPE, default: Debug
#-------------------------------------------------------------------------------
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE "Debug" CACHE string "Build type" FORCE)
endif (NOT CMAKE_BUILD_TYPE)
message (STATUS "Build type: " ${CMAKE_BUILD_TYPE})

#-------------------------------------------------------------------------------
# Find OpenSSL
#-------------------------------------------------------------------------------
find_package (OpenSSL REQUIRED)
include_directories (${OPENSSL_INCLUDE_DIR})
list(APPEND LIBS ${OPENSSL_LIBRARIES}) 

#-------------------------------------------------------------------------------
# Find CURL
#-------------------------------------------------------------------------------
find_package (CURL REQUIRED)
include_directories (${CURL_INCLUDE_DIR})
list(APPEND LIBS ${CURL_LIBRARIES})

#-------------------------------------------------------------------------------
# Find spdlog
#-------------------------------------------------------------------------------

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.13.0
)
FetchContent_MakeAvailable(spdlog)

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
include_directories(${ixwebsocket_SOURCE_DIR}/include)
list(APPEND LIBS ixwebsocket)

#-------------------------------------------------------------------------------
# A sensible json library
#-------------------------------------------------------------------------------

find_package(nlohmann_json REQUIRED)

#-------------------------------------------------------------------------------
# Subdirectories
#-------------------------------------------------------------------------------
add_subdirectory (src)
add_subdirectory (tests)