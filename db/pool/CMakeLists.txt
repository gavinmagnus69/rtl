cmake_minimum_required(VERSION 3.27)

project(libsdbpool)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(public_hpp
  ConnectionPool.hpp
  PoolConfig.hpp
  PoolState.hpp
  PooledConnection.hpp
)

set(private_cpp
  ConnectionPool.cpp
  PooledConnection.cpp
)


add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} PRIVATE
  ${private_cpp}
)

target_sources(${PROJECT_NAME} PUBLIC
  ${public_hpp}
)

target_link_libraries(${PROJECT_NAME} PUBLIC
  libsdbcore::libsdbcore
)
