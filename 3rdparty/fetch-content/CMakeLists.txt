# project(fetch-content)

# # # options can be moved out if here !!!!!!!!!!!

# # # Опции для опциональных зависимостей
# # option(WITH_JSON "Enable nlohmann-json support" ON)
# # option(WITH_SPDLOG "Enable spdlog support" ON)

# # ==================== Загрузка зависимостей ====================
# # if(WITH_JSON AND NOT TARGET nlohmann_json::nlohmann_json)
# #     find_package(nlohmann_json QUIET CONFIG)
# # endif()

# # if(WITH_JSON AND NOT TARGET nlohmann_json::nlohmann_json)
# #     message("Fetching nlohmann_json")
# #     include(FetchContent)
# #     set(FETCHCONTENT_QUIET ON)
# #     FetchContent_Declare(json
# #         GIT_REPOSITORY https://github.com/nlohmann/json.git
# #         GIT_TAG v3.12.0
# #         GIT_SHALLOW ON
# #     )
# #     FetchContent_MakeAvailable(json)
# # endif()


# # if(WITH_JSON AND NOT TARGET spdlog::spdlog)
# #     find_package(spdlog QUIET CONFIG)
# # endif()


# if(WITH_SPDLOG AND NOT TARGET spdlog::spdlog)
#     message("Fetching spdlog")

#     include(FetchContent)
#     set(FETCHCONTENT_QUIET ON)
    
#     FetchContent_Declare(
#     spdlog
#     GIT_REPOSITORY https://github.com/gabime/spdlog.git
#     GIT_TAG v1.14.1
#     GIT_SHALLOW TRUE
#     )


#     set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "Use std::format instead of bundled fmt" FORCE)
#     set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
#     set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
#     set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
#     FetchContent_MakeAvailable(spdlog)
# endif()



# if(WITH_JSON AND NOT TARGET nlohmann_json::nlohmann_json)
#     message(WARNING "nlohmann-json not found, disabling support")
#     set(WITH_JSON OFF)
# endif()
