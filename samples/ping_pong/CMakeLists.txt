cmake_minimum_required(VERSION 3.10)

project(execute_once LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(libael 0.2 REQUIRED)

add_executable(ping_server ping_server.cc)
target_link_libraries(ping_server ael pthread)
get_filename_component(PARENT_DIR ../ ABSOLUTE)
target_include_directories(ping_server PRIVATE ${PARENT_DIR})

add_executable(ping_client ping_client.cc)
target_link_libraries(ping_client ael pthread)
get_filename_component(PARENT_DIR ../ ABSOLUTE)
target_include_directories(ping_client PRIVATE ${PARENT_DIR})