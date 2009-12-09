add_subdirectory(x0)
add_subdirectory(plugins)

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

add_executable(x0d x0d.cpp)
target_link_libraries(x0d x0)

install(TARGETS x0d RUNTIME DESTINATION sbin)
install(FILES x0d.conf-dist DESTINATION etc)
