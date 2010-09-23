include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${ASIO_INCLUDEDIR})
include_directories(${BOOST_INCLUDEDIR})
include_directories(${LUA_INCLUDE_DIRS})

set(LLVM_CFLAGS -D_DEBUG -D_GNU_SOURCE -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS CACHE STRING "llvm cflags")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/flow/CMakeLists.txt)
	set(FLOW_CFLAGS -I${CMAKE_CURRENT_SOURCE_DIR} CACHE STRING "flow cflags")
	set(FLOW_LIBS flow CACHE STRING "flow cflags")

	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/flow)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/flow)
else()
	set(FLOW_CFLAGS -I/usr/include  CACHE STRING "flow cflags")
	set(FLOW_LIBS -lflow CACHE STRING "flow libs")
endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/flow/CMakeLists.txt)

add_definitions(
	${LLVM_CFLAGS}
	-Dlinux=1
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-DPACKAGE_NAME="${PACKAGE_NAME}"
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DLOCALSTATEDIR="${LOCALSTATEDIR}"
	-DPLUGINDIR="${PLUGINDIR}"
	-DVERSION="${PACKAGE_VERSION}")

add_executable(x0d x0d.cpp)
target_link_libraries(x0d x0)

install(TARGETS x0d RUNTIME DESTINATION sbin)
install(FILES x0d.conf-dist DESTINATION etc)

add_subdirectory(x0)
add_subdirectory(plugins)
