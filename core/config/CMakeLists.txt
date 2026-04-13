cmake_minimum_required(VERSION 3.27)

project(libcoreconfig)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

file(GLOB_RECURSE public_hpp  CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE private_cpp CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")


target_include_directories(${PROJECT_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} 
PUBLIC ${public_hpp}
PRIVATE ${private_cpp}
)


target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog)