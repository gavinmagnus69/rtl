project(example_ws)

add_executable(${PROJECT_NAME} main.cpp)

add_executable(example_client main_client.cpp)

target_compile_options(example_client PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
)

target_compile_options(${PROJECT_NAME} PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/bigobj>
  )

target_link_libraries(example_client PRIVATE 
librtlstp::librtlstp 
libcoreconfig::libcoreconfig
libcorelogger::libcorelogger
libswsserver::libswsserver
)


target_link_libraries(${PROJECT_NAME} PRIVATE 
librtlstp::librtlstp 
libcoreconfig::libcoreconfig
libcorelogger::libcorelogger
libswsserver::libswsserver 
libsapp::libsapp
)
