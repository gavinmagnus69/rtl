project(example-coro)

add_executable(example-coro main.cpp)

target_link_libraries(example-coro PRIVATE libsrtlcoro::libsrtlcoro)
