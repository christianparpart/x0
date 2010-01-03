include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${ASIO_INCLUDEDIR})
include_directories(${BOOST_INCLUDEDIR})

add_definitions(
	-Dlinux=1
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-DPACKAGE_NAME="${PACKAGE_NAME}"
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DVERSION="${PACKAGE_VERSION}")

add_executable(x0io_test
	io_test.cpp
	fd_source.cpp
	file_source.cpp
	buffer_source.cpp
	fd_sink.cpp
	file_sink.cpp
	buffer_sink.cpp
	filter.cpp
	chain_filter.cpp
	null_filter.cpp
)

#target_link_libraries(x0io_test x0)
target_link_libraries(x0io_test
	${CPPUNIT_LIBRARIES}
)
