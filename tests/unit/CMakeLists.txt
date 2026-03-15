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
