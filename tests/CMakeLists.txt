add_executable (private_method examples/private_method.cpp)
target_link_libraries (private_method krakenapi)

add_executable (public_method examples/public_method.cpp)
target_link_libraries (public_method krakenapi)

add_executable (public_ws examples/public_ws.cpp)
target_link_libraries (public_ws krakenapi)

add_executable (private_ws examples/private_ws.cpp)
target_link_libraries (private_ws krakenapi)
