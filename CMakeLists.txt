cmake_minimum_required (VERSION 3.15)
project (KRAKENAPI)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(INCLUDE_KCLIENT_APIS "should you build the json layer" OFF)

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

if(INCLUDE_KCLIENT_APIS)

#-------------------------------------------------------------------------------
# Add library kapi (krakenapi)
#-------------------------------------------------------------------------------

	aux_source_directory(kraken KAPI_FILES)
	add_library (kapi STATIC ${KAPI_FILES})

	# set some properties when CMAKE_BUILD_TYPE is "Debug"
	set_target_properties (kapi PROPERTIES 
				COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT"
				DEBUG_POSTFIX "d")

	list (INSERT LIBS 0 kapi)

endif()

#-------------------------------------------------------------------------------
# Add the executable 'kph'
#-------------------------------------------------------------------------------
if(INCLUDE_KCLIENT_APIS)

	add_executable (kph kph.cpp)
	set_target_properties (kph PROPERTIES 
				COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT")
	target_link_libraries (kph ${LIBS})

endif()

#-------------------------------------------------------------------------------
# Add the executable 'krt'
#-------------------------------------------------------------------------------
if(INCLUDE_KCLIENT_APIS)

	add_executable (krt krt.cpp)
	set_target_properties (krt PROPERTIES 
				COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT")
	target_link_libraries (krt ${LIBS})

endif()

#-------------------------------------------------------------------------------
# Add the executable 'private_method'
#-------------------------------------------------------------------------------
add_executable (private_method examples/private_method.cpp kapi.cpp)
set_target_properties (private_method PROPERTIES
		COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT")
target_link_libraries (private_method ${LIBS})

#-------------------------------------------------------------------------------
# Add the executable 'public_method'
#-------------------------------------------------------------------------------
add_executable (public_method examples/public_method.cpp kapi.cpp)
set_target_properties (public_method PROPERTIES
		COMPILE_DEFINITIONS_DEBUG "JSON_DEBUG;JSON_SAFE;JSON_ISO_STRICT")
target_link_libraries (public_method ${LIBS})

#-------------------------------------------------------------------------------
# Add the executable 'public_ws'
#-------------------------------------------------------------------------------
add_executable (public_ws examples/public_ws.cpp kapi.cpp)
target_link_libraries (public_ws ${LIBS} fmt::fmt)

#-------------------------------------------------------------------------------
# Add the executable 'private_ws'
#-------------------------------------------------------------------------------
add_executable (private_ws examples/private_ws.cpp kapi.cpp)
target_link_libraries (private_ws ${LIBS} fmt::fmt nlohmann_json::nlohmann_json)