add_executable(kraken_unit_tests
    test_rest_requests.cpp
    test_rest_responses.cpp
    test_client.cpp
    test_signature.cpp
)

target_link_libraries(kraken_unit_tests
    kapi
    krakenapi
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(kraken_unit_tests)

#-------------------------------------------------------------------------------
# WebSocket client unit tests (no network I/O – mock connection only)
#-------------------------------------------------------------------------------

add_executable(test_ws_client test_ws_client.cpp)
target_link_libraries(test_ws_client krakenapi GTest::gtest_main)
gtest_discover_tests(test_ws_client)

#-------------------------------------------------------------------------------
# WebSocket response parsing unit tests (sample JSON → from_json verification)
#-------------------------------------------------------------------------------

add_executable(test_ws_responses test_ws_responses.cpp)
target_link_libraries(test_ws_responses krakenapi GTest::gtest_main)
gtest_discover_tests(test_ws_responses)

#-------------------------------------------------------------------------------
# TickPrice unit tests
#-------------------------------------------------------------------------------

add_executable(test_tick_price test_tick_price.cpp)
target_link_libraries(test_tick_price krakenapi GTest::gtest_main)
gtest_discover_tests(test_tick_price)

#-------------------------------------------------------------------------------
# OrderType to_string/from_string unit tests (generic vs. Kraken wire format)
#-------------------------------------------------------------------------------

add_executable(test_order_type test_order_type.cpp)
target_link_libraries(test_order_type krakenapi GTest::gtest_main)
gtest_discover_tests(test_order_type)

#-------------------------------------------------------------------------------
# WsReconnectSession unit tests (no network I/O)
#-------------------------------------------------------------------------------

add_executable(test_ws_reconnect_session test_ws_reconnect_session.cpp)
target_include_directories(test_ws_reconnect_session PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(test_ws_reconnect_session GTest::gtest_main)
gtest_discover_tests(test_ws_reconnect_session)
