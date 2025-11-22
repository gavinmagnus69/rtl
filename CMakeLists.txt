# Roman Test Library
cmake_minimum_required(VERSION 3.27)

# librtl::librtl to link
project(librtl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure nlohmann_json target is available before consumers get added.
if(NOT TARGET nlohmann_json::nlohmann_json)
    message("Case 1")
    find_package(nlohmann_json QUIET CONFIG)
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    add_subdirectory(3rdparty)
    message("Case 3")
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    message(FATAL_ERROR "nlohmann_json::nlohmann_json is required but could not be located or fetched.")
    message("Case 2")
endif()

add_subdirectory(stp)
add_subdirectory(json)
# add_subdirectory(collectors)

# add_subdirectory(protocol)
add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} INTERFACE
    librtlstp::librtlstp
    librtljson::librtljson
    nlohmann_json::nlohmann_json

    # librtlprotocol::librtlprotocol
)
