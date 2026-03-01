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
