add_executable (private_rest examples/private_rest.cpp)
target_link_libraries (private_rest krakenapi)

add_executable (public_rest examples/public_rest.cpp)
target_link_libraries (public_rest krakenapi)

add_executable (public_ws examples/public_ws.cpp)
target_link_libraries (public_ws krakenapi)

add_executable (private_ws examples/private_ws.cpp)
target_link_libraries (private_ws krakenapi)
