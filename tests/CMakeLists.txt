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
	http11.cpp
	range_def_test.cpp
)

target_link_libraries(x0test
	boost_system boost_signals
	${LUA_LIBRARIES}
	${CPPUNIT_LIBRARIES}
	dl pthread
)

install(TARGETS x0test
	RUNTIME DESTINATION sbin
)

# -----------------------------------------------------------------
add_executable(x0test_common
	x0dtest.cpp
	ternary_search_test.cpp
	buffer_test.cpp
)

target_link_libraries(x0test_common
	${CPPUNIT_LIBRARIES}
	${LUA_LIBRARIES}
)
