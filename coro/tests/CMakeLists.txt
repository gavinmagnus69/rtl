project(test-coro)

add_executable(test-coro task-test.cpp)

target_link_libraries(test-coro PUBLIC libsrtlcoro::libsrtlcoro)
