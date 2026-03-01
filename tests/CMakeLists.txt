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
# Example apps
#-------------------------------------------------------------------------------

add_executable(private_rest examples/private_rest.cpp)
target_link_libraries(private_rest krakenapi spdlog::spdlog)

add_executable (public_rest examples/public_rest.cpp)
target_link_libraries(public_rest krakenapi spdlog::spdlog)

add_executable (public_ws examples/public_ws.cpp)
target_link_libraries(public_ws krakenapi ixwebsocket spdlog::spdlog)

add_executable (private_ws examples/private_ws.cpp)
target_link_libraries(private_ws krakenapi ixwebsocket spdlog::spdlog)
