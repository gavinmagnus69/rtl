message("Fetching zlib")


include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG v1.3.1
    GIT_SUBMODULES ""
    GIT_SUBMODULES_RECURSE FALSE
)

set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(zlib)