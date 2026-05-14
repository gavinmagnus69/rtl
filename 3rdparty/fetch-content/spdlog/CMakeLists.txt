  message("Fetching spdlog")

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(
spdlog
GIT_REPOSITORY https://github.com/gabime/spdlog.git
GIT_TAG v1.14.1
GIT_SHALLOW TRUE
)


set(SPDLOG_USE_STD_FORMAT ON CACHE BOOL "Use std::format instead of bundled fmt" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)