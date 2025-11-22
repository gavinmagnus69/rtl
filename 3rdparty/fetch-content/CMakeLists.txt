project(fetch-content)

# options can be moved out if here !!!!!!!!!!!

# Опции для опциональных зависимостей
option(WITH_JSON "Enable nlohmann-json support" ON)

# ==================== Загрузка зависимостей ====================
if(WITH_JSON AND NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json QUIET CONFIG)
endif()

if(WITH_JSON AND NOT TARGET nlohmann_json::nlohmann_json)
    include(FetchContent)
    set(FETCHCONTENT_QUIET ON)
    FetchContent_Declare(json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW ON
    )
    FetchContent_MakeAvailable(json)
endif()

if(WITH_JSON AND NOT TARGET nlohmann_json::nlohmann_json)
    message(WARNING "nlohmann-json not found, disabling support")
    set(WITH_JSON OFF)
endif()
