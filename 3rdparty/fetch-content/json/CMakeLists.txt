message("Fetching nlohmann_json")
include(FetchContent)
set(FETCHCONTENT_QUIET ON)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(json)