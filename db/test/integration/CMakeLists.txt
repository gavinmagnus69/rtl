cmake_minimum_required(VERSION 3.21)
project(db_integration_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libpqxx CONFIG REQUIRED)

add_executable(example_db_core_integration_test main.cpp)
target_link_libraries(example_db_core_integration_test PRIVATE
  libsdbcore::libsdbcore
  libpqxx::pqxx
)

add_executable(example_db_pool_integration_test pool_main.cpp)
target_link_libraries(example_db_pool_integration_test PRIVATE
  libsdbpool::libsdbpool
  libsdbcore::libsdbcore
  libpqxx::pqxx
)

add_executable(example_db_tx_integration_test tx_main.cpp)
target_link_libraries(example_db_tx_integration_test PRIVATE
  spdlog::spdlog
  libsdbtx::libsdbtx
  libsdbpool::libsdbpool
  libsdbcore::libsdbcore
  libpqxx::pqxx
)
