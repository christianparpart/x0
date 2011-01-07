#set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(../src)
include_directories(${ASIO_INCLUDEDIR})
include_directories(${BOOST_INCLUDEDIR})

add_definitions(
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DVERSION="${PACKAGE_VERSION}")

# -----------------------------------------------------------------
add_executable(x0test
	x0dtest.cpp
	range_def_test.cpp
	message_processor_test.cpp
	strutils_test.cpp
	ternary_search_test.cpp
	buffer_test.cpp
	io_test.cpp
	#http11.cpp
)

target_link_libraries(x0test
	boost_system boost_signals
	${LUA_LIBRARIES}
	${CPPUNIT_LIBRARIES}
	dl pthread
	x0
)

# -----------------------------------------------------------------
# some quicky executables

#add_executable(x0test_copy io_copy.cpp)
#target_link_libraries(x0test_copy x0)

add_executable(web_client_test web_client_test.cpp)
target_link_libraries(web_client_test ${CPPUNIT_LIBRARIES} x0)

add_executable(fcgi-long-run fcgi-long-run.cpp)
target_link_libraries(fcgi-long-run fcgi)

add_executable(fcgi-staticfile fcgi-staticfile.cpp)
target_link_libraries(fcgi-staticfile fcgi)
