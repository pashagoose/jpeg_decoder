add_library(decoder

	context.cpp
	marker_controller.cpp
	marker_handlers.cpp
	bitreader.cpp

        huffman.cpp
        fft.cpp
        decoder.cpp)

target_include_directories(decoder PUBLIC
        ${FFTW_INCLUDES}
	)

target_link_libraries(decoder PUBLIC
        ${FFTW_LIBRARIES}
        glog::glog)

get_target_property(GLOG_INCLUDES glog::glog INCLUDE_DIRECTORIES)
target_include_directories(decoder SYSTEM PUBLIC ${GLOG_INCLUDES})

