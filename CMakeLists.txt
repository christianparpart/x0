cmake_minimum_required(VERSION 2.6)

project(x0)
set(PACKAGE_NAME "x0")
set(PACKAGE_VERSION "0.3.0")
set(PACKAGE_HOMEPAGE_URL "http://redmine.xzero.ws/projects/x0")
set(PACKAGE_BUGTRACKER_URL "http://redmine.xzero.ws/projects/x0/issues")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FindPkgConfig)
include(FindDoxygen)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CMakeDetermineCCompiler)

#option(BUILD_STATIC "Build with modules statically linked in [default: off]")
option(BUILD_TESTS "Build unit tests [default: on]" ON)

option(WITH_INOTIFY "Build with inotify support [default: on]" ON)
#option(WITH_LIBAIO "Build with libaio support [default: on]" ON)
#option(WITH_PCRE "With PCRE support [default: off]")
option(WITH_SSL "With SSL support [default: on]" ON)
option(WITH_CONNECTION_TIMEOUTS "Connection read/write timeout support [default: on]" ON)

# we need the following definitions in order to get some special
# OS-level features like posix_fadvise() or readahead().
add_definitions(-DXOPEN_SOURCE=600)
add_definitions(-DGNU_SOURCE)

# enforce 64bit i/o operations, even on 32bit platforms
add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES)

CHECK_INCLUDE_FILES(fcntl.h HAVE_FCNTL_H)
CHECK_INCLUDE_FILES(netdb.h HAVE_NETDB_H)

CHECK_INCLUDE_FILES(sys/sendfile.h HAVE_SYS_SENDFILE_H)
CHECK_FUNCTION_EXISTS(sendfile HAVE_SENDFILE)
CHECK_FUNCTION_EXISTS(posix_fadvise HAVE_POSIX_FADVISE)
CHECK_FUNCTION_EXISTS(readahead HAVE_READAHEAD)

CHECK_INCLUDE_FILES(sys/resource.h HAVE_SYS_RESOURCE_H)
CHECK_INCLUDE_FILES(sys/mman.h HAVE_SYS_MMAN_H)
CHECK_INCLUDE_FILES(sys/limits.h HAVE_SYS_LIMITS_H)
CHECK_INCLUDE_FILES(pwd.h HAVE_PWD_H)
CHECK_INCLUDE_FILES(syslog.h HAVE_SYSLOG_H)
CHECK_INCLUDE_FILES(uuid/uuid.h HAVE_UUID_UUID_H)
CHECK_INCLUDE_FILES(sys/utsname.h HAVE_SYS_UTSNAME_H)

CHECK_FUNCTION_EXISTS(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS(chroot HAVE_CHROOT)
CHECK_FUNCTION_EXISTS(pathconf HAVE_PATHCONF)

if(WITH_INOTIFY)
	CHECK_INCLUDE_FILES(sys/inotify.h HAVE_SYS_INOTIFY_H)
	if(HAVE_SYS_INOTIFY_H)
		CHECK_FUNCTION_EXISTS(inotify_init1 HAVE_INOTIFY_INIT1)
	endif(HAVE_SYS_INOTIFY_H)
endif(WITH_INOTIFY)

CHECK_INCLUDE_FILES(zlib.h HAVE_ZLIB_H)
if(HAVE_ZLIB_H)
	CHECK_LIBRARY_EXISTS(z deflate "" HAVE_LIBZ)
	set(ZLIB_LIBRARIES z)
	set(LIBS ${LIBS} ${ZLIB_LIBRARIES})
endif(HAVE_ZLIB_H)

CHECK_INCLUDE_FILES(bzlib.h HAVE_BZLIB_H)
if(HAVE_BZLIB_H)
	CHECK_LIBRARY_EXISTS(bz2 BZ2_bzCompressInit "" HAVE_LIBBZ2)
	set(BZIP2_LIBRARIES bz2)
	set(LIBS ${LIBS} ${BZIP2_LIBRARIES})
endif(HAVE_BZLIB_H)

# flow
find_package(FLOW)
include_directories(${FLOW_INCLUDE_DIR})
link_directories(${FLOW_LIBRARY_DIR})
add_definitions(${FLOW_CPPFLAGS})
set(LIBS ${LIBS} ${FLOW_LIBRARIES})

# ev
find_package(EV)
include_directories(${EV_INCLUDE_DIR})
link_directories(${EV_LIBRARY_DIR})
add_definitions(${EV_CPPFLAGS})
set(LIBS ${LIBS} ${EV_LIBRARIES})

# {{{ path prefixes
# ---------------------------------------------------------------------------
if (DEFINED CMAKE_INSTALL_PREFIX)
	set(SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc CACHE PATH "x0d configuration directory")
	set(INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include CACHE PATH "x0 include directory")
	set(LIBDIR ${CMAKE_INSTALL_PREFIX}/lib CACHE PATH "x0 library directory")
	set(LOCALSTATEDIR ${CMAKE_INSTALL_PREFIX}/var CACHE PATH "x0d run-time state directory")
endif()

if (DEFINED SYSCONFDIR)
	set(SYSCONFDIR ${SYSCONFDIR} CACHE PATH "configuration directory")
else()
	set(SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc CACHE PATH "configuration directory")
endif()

if (DEFINED INCLUDEDIR)
	set(INCLUDEDIR ${INCLUDEDIR} CACHE PATH "path to library includes")
else()
	set(INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include CACHE PATH "path to library includes")
endif()

if (DEFINED LIBDIR)
	set(LIBDIR ${LIBDIR} CACHE PATH "path to program libraries")
else()
	set(LIBDIR ${CMAKE_INSTALL_PREFIX}/lib CACHE PATH "path to program libraries")
endif()

set(PLUGINDIR "${LIBDIR}/x0" CACHE PATH "path to x0 plugins")

if (DEFINED LOCALSTATEDIR)
	set(LOCALSTATEDIR ${LOCALSTATEDIR} CACHE PATH "run-time state directory")
else()
	set(LOCALSTATEDIR ${CMAKE_INSTALL_PREFIX}/var CACHE PATH "run-time state directory")
endif()
# ---------------------------------------------------------------------------
# }}}

FIND_PACKAGE(Boost REQUIRED)
if(Boost_FOUND)
	message(STATUS "Boost library directory: " ${Boost_LIBRARY_DIRS})
	message(STATUS "Boost include directory: " ${Boost_INCLUDE_DIRS})
endif(Boost_FOUND)

#set(CMAKE_VERBOSE_MAKEFILE ON)

pkg_check_modules(LUA REQUIRED lua>=5.1)
message(STATUS "LUA include directory: " ${LUA_INCLUDE_DIRS})
message(STATUS "LUA libraries: " ${LUA_LIBRARIES})

if(WITH_SSL)
	pkg_check_modules(GNUTLS REQUIRED gnutls>=2.0.0 gnutls-extra>=2.0.0)
endif(WITH_SSL)
if(BUILD_TESTS)
	pkg_check_modules(CPPUNIT REQUIRED cppunit>=1.12)
endif(BUILD_TESTS)

#if(BUILD_STATIC)
	# ...
#else(BUILD_STATIC)
	CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
#endif(BUILD_STATIC)

# ---------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src)

# ---------------------------------------------------------------------------
macro(x0_plugin_simple ModuleName)
	add_library(${ModuleName} MODULE ${ModuleName}.cpp)
	set_target_properties(${ModuleName} PROPERTIES PREFIX "")
	install(TARGETS ${ModuleName} DESTINATION ${LIBDIR}/x0)
endmacro()

macro(x0_plugin ModuleName SrcFiles)
	add_library(${ModuleName} MODULE ${SrcFiles})
	set_target_properties(${ModuleName} PROPERTIES PREFIX "")
	install(TARGETS ${ModuleName} DESTINATION ${LIBDIR}/x0)
endmacro()

# ---------------------------------------------------------------------------
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/x0/sysconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/x0/sysconfig.h)
add_definitions(-DHAVE_SYSCONFIG_H)

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)
