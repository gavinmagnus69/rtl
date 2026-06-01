project(test-coro)

add_executable(test-coro task-test.cpp)

target_link_libraries(test-coro PUBLIC libsrtlcoro::libsrtlcoro)

add_executable(test-coro-scheduler scheduler-test.cpp)

target_link_libraries(test-coro-scheduler PUBLIC libsrtlcoro::libsrtlcoro)
