message("Fetching boost")


include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(
  boost
  GIT_REPOSITORY https://github.com/boostorg/boost.git
  GIT_TAG        boost-1.87.0
  GIT_PROGRESS ON
  GIT_SHALLOW ON
  OVERRIDE_FIND_PACKAGE TRUE # needed to find correct Boost
  EXCLUDE_FROM_ALL
)

set(BOOST_SUPERPROJECT_BUILD_TESTS OFF)

FetchContent_MakeAvailable(boost)