add_library(ael_openssl SHARED 
	ssl_stream_buffer_filter.cc)

target_include_directories(ael_openssl 
	PUBLIC ${PROJECT_SOURCE_DIR}/include
	PUBLIC ${CMAKE_BINARY_DIR}/include
	PRIVATE ${PROJECT_SOURCE_DIR}/lib
	PRIVATE ${OPENSSL_INCLUDE_DIR})