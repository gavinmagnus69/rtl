project(librtlstp)

add_library(${PROJECT_NAME} STATIC
    StaticThreadPool.h
    StaticThreadPool.cpp
    UnboundedMPMCQueue.h
    RtlThreadPool.hpp
    RtlThreadPool.cpp
    IExecutor.hpp
    IExecutor.cpp
    ThreadPoolExecutor.hpp
    ThreadPoolExecutor.cpp
)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
