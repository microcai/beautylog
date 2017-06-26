
add_library(beautylog STATIC
	include/beautylog/beautylog.hpp
	src/beautylog.cpp
)

target_link_libraries(beautylog utility Boost::system Boost::thread Boost::regex Boost::locale
	${CMAKE_THREAD_LIBS_INIT} dl systemd)

target_include_directories(beautylog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(beautylog  PROPERTIES FOLDER "libs")