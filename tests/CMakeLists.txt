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

if(BUILD_TESTS)
# -----------------------------------------------------------------
add_executable(x0test
	x0dtest.cpp
	range_def_test.cpp
	message_processor_test.cpp
	strutils_test.cpp
	ternary_search_test.cpp
	buffer_test.cpp
	io_test.cpp
	base64_test.cpp
	#http11.cpp
)

target_link_libraries(x0test
	${LUA_LIBRARIES}
	${CPPUNIT_LIBRARIES}
	dl pthread
	XzeroBase
	XzeroHttp
)

# -----------------------------------------------------------------

add_executable(flow_test flow_test.cpp)
target_link_libraries(flow_test
	${CPPUNIT_LIBRARIES}
	dl pthread
	XzeroBase
	XzeroFlow
)

add_executable(test_flow_lexer test_flow_lexer.cpp)
target_link_libraries(test_flow_lexer
	${CPPUNIT_LIBRARIES}
	dl pthread
	XzeroBase
	XzeroFlow
)


# -----------------------------------------------------------------
# some quicky executables

add_executable(x0cp x0cp.cpp)
target_link_libraries(x0cp XzeroBase)

#add_executable(web_client_test web_client_test.cpp)
#target_link_libraries(web_client_test ${CPPUNIT_LIBRARIES} XzeroHttp)

add_executable(fcgi-long-run fcgi-long-run.cpp)
set_target_properties(fcgi-long-run PROPERTIES COMPILE_FLAGS "${CFLAGS} -I${FCGI_INCLUDE_PATH}")
target_link_libraries(fcgi-long-run ${FCGI_LIBRARY_PATH})

add_executable(fcgi-staticfile fcgi-staticfile.cpp)
set_target_properties(fcgi-staticfile PROPERTIES COMPILE_FLAGS "${CFLAGS} -I${FCGI_INCLUDE_PATH}")
target_link_libraries(fcgi-staticfile ${FCGI_LIBRARY_PATH})

add_executable(splice_test splice_test.cpp)
target_link_libraries(splice_test XzeroBase)

endif(BUILD_TESTS)
