#set(CMAKE_VERBOSE_MAKEFILE ON)

include_directories(../src)
include_directories(${BOOST_INCLUDEDIR})

add_definitions(
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DVERSION="${PACKAGE_VERSION}")

add_executable(x0test
	x0dtest.cpp
	http11.cpp
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
