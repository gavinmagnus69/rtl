project(libsrtldb)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(WITH_DB_TESTS OFF)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})




add_subdirectory(core)
add_subdirectory(pool)
add_subdirectory(tx)


target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtldbcore::libsrtldbcore)
target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtldbpool::libsrtldbpool)
target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtldbtx::libsrtldbtx)



if(WITH_DB_TESTS) 
    add_subdirectory(test)
endif()