project(libsrtlcore)

option(WITH_CORE_LOGGER "Include logger module to libsrtlcore interface" ON)
option(WITH_CORE_CONFIG "Include config module to libsrtlcore interface" ON)

add_subdirectory(config)
add_subdirectory(logger)


add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})


if(WITH_CORE_CONFIG)
    target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtlcoreconfig::libsrtlcoreconfig)
endif()

if(WITH_CORE_LOGGER)
    target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtlcorelogger::libsrtlcorelogger)
endif()

