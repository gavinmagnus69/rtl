project(libswstest)

add_executable(WsProtocolTest
  WsProtocolTest.cpp
)

add_executable(WsAppLogicTest
  WsAppLogicTest.cpp
)

add_executable(AppEmulationTest
  AppEmulationTest.cpp
)

add_executable(AppDatabaseIntegrationTest
  AppDatabaseIntegrationTest.cpp
)

target_compile_options(WsAppLogicTest PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
)

target_compile_options(AppEmulationTest PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
)

target_compile_options(AppDatabaseIntegrationTest PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
)

target_include_directories(WsProtocolTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../server
)

target_include_directories(WsAppLogicTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../server
)

target_include_directories(AppEmulationTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../server
)

target_include_directories(AppDatabaseIntegrationTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../server
)

target_link_libraries(WsProtocolTest PRIVATE
  libswsserver::libswsserver
  nlohmann_json::nlohmann_json
)

target_link_libraries(WsAppLogicTest PRIVATE
  libswsserver::libswsserver
  nlohmann_json::nlohmann_json
)

target_link_libraries(AppEmulationTest PRIVATE
  libsapp::libsapp
  libswsdto::libswsdto
  nlohmann_json::nlohmann_json
)

target_link_libraries(AppDatabaseIntegrationTest PRIVATE
  libsapp::libsapp
  libswsdto::libswsdto
  libsdbcore::libsdbcore
  nlohmann_json::nlohmann_json
)
