
if(WITH_JSON_BUILD OR WITH_CORE_BUILD OR WITH_WS_BUILD)
    find_package(nlohmann_json CONFIG QUIET)
    if(NOT TARGET nlohmann_json::nlohmann_json AND RTL_FETCH_MISSING_DEPS)
      add_subdirectory(fetch-content/json)
    endif()
    rtl_require_target(nlohmann_json::nlohmann_json "json/core/ws modules")
  endif()

  if(WITH_CORE_BUILD OR WITH_WS_BUILD)
    find_package(spdlog CONFIG QUIET)
    if(NOT TARGET spdlog::spdlog AND RTL_FETCH_MISSING_DEPS)
      add_subdirectory(fetch-content/spdlog)
    endif()
    rtl_require_target(spdlog::spdlog "core/ws modules")
  endif()

  if(WITH_DB_BUILD)
    find_package(libpqxx CONFIG REQUIRED)
    rtl_require_target(libpqxx::pqxx "db module")
  endif()

  if(WITH_WS_BUILD)
    # find_package(Boost REQUIRED)
    if(NOT TARGET Boost::beast AND RTL_FETCH_MISSING_DEPS)
      add_subdirectory(fetch-content/boost)
    endif()
    rtl_require_target(Boost::beast "ws module")
    if(NOT TARGET zlib AND NOT TARGET zlibstatic AND RTL_FETCH_MISSING_DEPS) 
      add_subdirectory(fetch-content/zlib)
    endif()

  endif()



# add_subdirectory(fetch-content)