project(librtljson)

find_package(nlohmann_json CONFIG QUIET)

add_library(${PROJECT_NAME} STATIC
    JsonFunctions.h
    JsonFunctions.cpp
)

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(${PROJECT_NAME} PUBLIC nlohmann_json::nlohmann_json)