cmake_minimum_required(VERSION 3.10)

project(execute_once LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(execute_once execute_once.cc)

find_package(libael 0.2 REQUIRED)

target_link_libraries(execute_once ael pthread)

get_filename_component(PARENT_DIR ../ ABSOLUTE)
target_include_directories(execute_once PRIVATE ${PARENT_DIR})