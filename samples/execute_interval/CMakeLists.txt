cmake_minimum_required(VERSION 3.10)

project(execute_once LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(execute_interval execute_interval.cc)

find_package(libael 0.2 REQUIRED)

target_link_libraries(execute_interval ael pthread)

get_filename_component(PARENT_DIR ../ ABSOLUTE)
target_include_directories(execute_interval PRIVATE ${PARENT_DIR})