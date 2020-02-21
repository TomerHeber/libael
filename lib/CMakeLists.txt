add_library(ael 
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
	PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
	PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include> $<INSTALL_INTERFACE:include>)

install(TARGETS ael 
	DESTINATION ${CMAKE_INSTALL_LIBDIR}
	EXPORT libael_targets)

install(FILES 
	${PROJECT_SOURCE_DIR}/include/data_view.h 
	${PROJECT_SOURCE_DIR}/include/event_loop.h 
	${PROJECT_SOURCE_DIR}/include/event.h
	${PROJECT_SOURCE_DIR}/include/handle.h
	${PROJECT_SOURCE_DIR}/include/log.h
	${PROJECT_SOURCE_DIR}/include/stream_buffer.h
	${PROJECT_SOURCE_DIR}/include/stream_listener.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ael)