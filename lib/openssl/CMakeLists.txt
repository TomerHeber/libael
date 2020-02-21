add_library(ael_openssl 
	ssl_stream_buffer_filter.cc)

target_include_directories(ael_openssl 
	PUBLIC ${PROJECT_SOURCE_DIR}/include
	PUBLIC ${CMAKE_BINARY_DIR}/include
	PRIVATE ${PROJECT_SOURCE_DIR}/lib
	PRIVATE ${OPENSSL_INCLUDE_DIR})

install(TARGETS ael_openssl DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${PROJECT_SOURCE_DIR}/include/ssl_stream_buffer_filter.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libael)