project(test-feature)


add_executable(test-feature feature-test.cpp)

target_link_libraries(test-feature PUBLIC 
librtlstp::librtlstp 
librtlallocators::librtlallocators
)