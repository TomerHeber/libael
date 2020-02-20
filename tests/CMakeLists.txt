#add_subdirectory(/googletest/googletest googletest)
#find_package(GTest REQUIRED PATHS ${CMAKE_BINARY_DIR}/tests/googletest)

#add_executable(data_view data_view_test.cc)
#target_link_libraries(data_view ${GTEST_BOTH_LIBRARIES})

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

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
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

if (OPENSSL_FOUND)
add_executable(ssl ssl_test.cc helpers.cc)
target_link_libraries(ssl ael ael_openssl gtest_main ${OPENSSL_SSL_LIBRARY})
add_test(NAME ssl_test COMMAND ssl)
endif()
