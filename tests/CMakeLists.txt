configure_file(CMakeLists.txt.googletest.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
	RESULT_VARIABLE result
 	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
 	message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )

if(result)
	message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)
                 
add_executable(data_view data_view_test.cc helpers.cc)
target_link_libraries(data_view ael gtest_main)
add_test(NAME data_view_test COMMAND data_view)

add_executable(execute execute_test.cc helpers.cc)
target_link_libraries(execute ael gtest_main)
add_test(NAME execute_test COMMAND execute)

add_executable(tcp tcp_test.cc helpers.cc)
target_link_libraries(tcp ael gtest_main)
add_test(NAME tcp_test COMMAND tcp)
