project(libsrtlws)

option(BUILD_WS_EXAMPLES "Build WebSocket examples" OFF)
option(BUILD_WS_TESTS "Build websocket tests" OFF)

rtl_require_target(Boost::beast "Beast module required")
rtl_require_target(libsrtlcore::libsrtlcore "Core module required")
rtl_require_target(libsrtlmiddlewareauth::libsrtlmiddlewareauth "Middleware module required")
rtl_require_target(librtlstp::librtlstp "Stp module required")
rtl_require_target(nlohmann_json::nlohmann_json "nlohmann module required")
rtl_require_target(spdlog::spdlog "spdlog module required")


add_subdirectory(server)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} INTERFACE  libsrtlwsserver::libsrtlwsserver)


if(BUILD_WS_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_WS_TESTS)
    add_subdirectory(test)
endif()

