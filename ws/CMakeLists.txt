set(BUILD_WS_EXAMPLES OFF)
option(BUILD_WS_TESTS "Build websocket tests" OFF)

add_subdirectory(server)
# add_subdirectory(dto)


if(BUILD_WS_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_WS_TESTS)
    add_subdirectory(test)
endif()

