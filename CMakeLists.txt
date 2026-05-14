# Roman Test Library
cmake_minimum_required(VERSION 3.27)

include(cmake/help.cmake)

# librtl::librtl to link
project(librtl)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")




# Selection of modules
option(WITH_TESTS_BUILD "Include High-level tests" ON)

option(WITH_STP_BUILD "Include STP" ON)

option(WITH_JSON_BUILD "Include JSON" ON)

option(WITH_ALLOCATORS_BUILD "Include Allocators" ON)

option(WITH_CORE_BUILD "Include Core" ON)

option(WITH_MIDDLEWARE_BUILD "Include Middleware" ON)

option(WITH_WS_BUILD "Include WebSocket" OFF)

option(WITH_DB_BUILD "Include Database module" ON)

option(RTL_FETCH_MISSING_DEPS "Fetch missing dependencies" ON)



add_subdirectory(3rdparty)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})



if(WITH_STP_BUILD)
    message("Configuring STP module")
    add_subdirectory(stp)
    target_link_libraries(${PROJECT_NAME} INTERFACE librtlstp::librtlstp)
endif()


if(WITH_JSON_BUILD)
    message("Configuring Json module")
    add_subdirectory(json)
    target_link_libraries(${PROJECT_NAME} INTERFACE librtljson::librtljson)
endif()

if(WITH_ALLOCATORS_BUILD)
    message("Configuring Allocators module")
    add_subdirectory(allocators)
    target_link_libraries(${PROJECT_NAME} INTERFACE librtlallocators::librtlallocators)
endif()

if(WITH_CORE_BUILD)
    message("Configuring Core module")
    add_subdirectory(core)
    target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtlcoreconfig::libsrtlcoreconfig
)
endif()


if(WITH_MIDDLEWARE_BUILD)
    message("Configuring Middleware module")
    add_subdirectory(middleware)
    target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtlmiddlewareauth::libsrtlmiddlewareauth
)
endif()


if(WITH_DB_BUILD)
    message("Configuring Middleware module")
    add_subdirectory(db)
    # target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtlmiddlewareauth::libsrtlmiddlewareauth)
endif()



if(WITH_TESTS_BUILD)
    add_subdirectory(test)
endif()
