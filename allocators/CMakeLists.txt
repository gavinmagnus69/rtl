project(librtlallocators)

add_library(${PROJECT_NAME} INTERFACE)

file(GLOB_RECURSE  hpp_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

target_sources(${PROJECT_NAME} PUBLIC ${hpp_files}) 

add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
