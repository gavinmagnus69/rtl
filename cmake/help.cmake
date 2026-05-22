 function(rtl_require_target target reason)
    if(NOT TARGET ${target})
      message(FATAL_ERROR "${target} is required: ${reason}")
    endif()
  endfunction()

if(WIN32 AND DEFINED ENV{VCPKG_ROOT})
    message("Setting vcpkg")
    set(VCPKG_ROOT_REAL "C:/Users/r.ahmetov/Desktop/personal/tst/vcpkg")
    message( "${VCPKG_ROOT_REAL}/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE
        "${VCPKG_ROOT_REAL}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()