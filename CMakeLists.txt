#Roman Test Library
cmake_minimum_required(VERSION 3.27)

# librtl::librtl to link
project(librtl)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(stp)
add_subdirectory(json)
# add_subdirectory(protocol)



add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} INTERFACE 
librtlstp::librtlstp 
librtljson::librtljson
# librtlprotocol::librtlprotocol
)
    



    
