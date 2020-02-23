cmake_minimum_required(VERSION 3.10)

project(libael
	VERSION 0.2.0
	DESCRIPTION "asynchronous event loop library"
	LANGUAGES CXX)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

include(GNUInstallDirs)

include(InstallRequiredSystemLibraries)
set(CPACK_PROJECT_URL "https://github.com/TomerHeber/libael")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VERSION_MAJOR "${libael_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${libael_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${libael_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT "heber.tomer@gmail.com")
set(CPACK_PACKAGE_VENDOR, "Tomer Heber")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
include(CPack)

include(CheckIncludeFileCXX)
check_include_file_cxx(unistd.h HAVE_UNISTD_H)
check_include_file_cxx(sys/types.h HAVE_SYS_SOCKET_H)
check_include_file_cxx(sys/socket.h HAVE_SYS_TYPES_H)
check_include_file_cxx(sys/epoll.h HAVE_SYS_EPOLL_H)
check_include_file_cxx(sys/eventfd.h HAVE_SYS_EVENTFD_H)
check_include_file_cxx(sys/timerfd.h HAVE_SYS_TIMERFD_H)
check_include_file_cxx(arpa/inet.h HAVE_ARPA_INET_H)

include(CheckSymbolExists)
check_symbol_exists(accept4 sys/socket.h HAVE_ACCEPT4)

configure_file(config.h.in include/config.h)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_compile_options(-Wall -Wextra -pedantic -g -O2)
endif()

add_subdirectory(lib)

include(CTest)
if(BUILD_TESTING)
	add_subdirectory(tests)
endif()

install(EXPORT libael_targets
	FILE libael_targets.cmake
	DESTINATION lib/cmake/libael)

include(CMakePackageConfigHelpers)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
	"${CMAKE_CURRENT_BINARY_DIR}/libaelConfig.cmake"
	INSTALL_DESTINATION "lib/cmake/libael"
	NO_SET_AND_CHECK_MACRO
	NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/libaelConfigVersion.cmake"
	VERSION "${libael_VERSION_MAJOR}.${libael_VERSION_MINOR}.${libael_VERSION_PATCH}"
	COMPATIBILITY AnyNewerVersion)

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/libaelConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/libaelConfigVersion.cmake
	DESTINATION lib/cmake/libael)
