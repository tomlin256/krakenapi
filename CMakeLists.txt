cmake_minimum_required (VERSION 3.15)
project (KRAKENAPI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#-------------------------------------------------------------------------------
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-Wno-tautological-undefined-compare)
endif()

#-------------------------------------------------------------------------------
IF (APPLE)
	set (CMAKE_MODULE_LINKER_FLAGS "-Wl,-flat_namespace -Wl,-undefined -Wl,suppress")
ENDIF (APPLE)

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
# Add ixwebsocket
#-------------------------------------------------------------------------------

include(FetchContent)

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
# Add fmt for std::format
#-------------------------------------------------------------------------------

find_package(fmt REQUIRED)

#-------------------------------------------------------------------------------
# A sensible json library
#-------------------------------------------------------------------------------

find_package(nlohmann_json REQUIRED)

#-------------------------------------------------------------------------------
# Add the examples using both REST and WebSockets
#-------------------------------------------------------------------------------
add_executable (private_method examples/private_method.cpp kapi.cpp)
target_link_libraries (private_method ${LIBS})

add_executable (public_method examples/public_method.cpp kapi.cpp)
target_link_libraries (public_method ${LIBS})

add_executable (public_ws examples/public_ws.cpp kapi.cpp)
target_link_libraries (public_ws ${LIBS} fmt::fmt)

add_executable (private_ws examples/private_ws.cpp kapi.cpp)
target_link_libraries (private_ws ${LIBS} fmt::fmt nlohmann_json::nlohmann_json)