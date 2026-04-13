cmake_minimum_required(VERSION 3.27)

project(libsdbobs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE public_hpp CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE private_cpp CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

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

# target_link_libraries(${PROJECT_NAME} PUBLIC
#   libcoreconfig::libcoreconfig
#   libcorelogger::libcorelogger
# )
