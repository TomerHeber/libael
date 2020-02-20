add_library(ael SHARED 
	config.cc 
	data_view.cc 
	event_loop.cc 
	event.cc 
	stream_buffer.cc 
	stream_listener.cc
	tcp_stream_buffer_filter.cc
	epoll.cc
	handle.cc
	log.cc)

target_include_directories(ael 
	PUBLIC ${PROJECT_SOURCE_DIR}/include
	PUBLIC ${CMAKE_BINARY_DIR}/include)
