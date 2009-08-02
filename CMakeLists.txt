cmake_minimum_required(VERSION 2.6)

project(x0)
set(PACKAGE_NAME "x0")
set(PACKAGE_VERSION "0.1.0")
set(PACKAGE_HOMEPAGE_URL "http://code.ninchens.net/projects/x0")
set(PACKAGE_BUGTRACKER_URL "http://code.ninchens.net/projects/x0/issues")

INCLUDE(FindPkgConfig)
INCLUDE(FindDoxygen)
INCLUDE(CheckIncludeFiles)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckVariableExists)
INCLUDE(CheckTypeSize)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CMakeDetermineCCompiler)

add_subdirectory(src)
add_subdirectory(tests)

OPTION(WITH_LUA "With LUA 5.1+ support [default: off]")
OPTION(WITH_PCRE "With PCRE support [default: off]")
OPTION(WITH_SSL "With SSL support [default: on]" ON)
OPTION(BUILD_STATIC "Build with modules statically linked in [default: off]")
OPTION(BUILD_TESTS "Build unit tests [default: on]" ON)

CHECK_INCLUDE_FILES(sys/sendfile.h HAVE_SYS_SENDFILE_H)
CHECK_INCLUDE_FILES(sys/resource.h HAVE_SYS_RESOURCE_H)
CHECK_INCLUDE_FILES(sys/mman.h HAVE_SYS_MMAN_H)
CHECK_INCLUDE_FILES(sys/limits.h HAVE_SYS_LIMITS_H)
CHECK_INCLUDE_FILES(pwd.h HAVE_PWD_H)
CHECK_INCLUDE_FILES(syslog.h HAVE_SYSLOG_H)
CHECK_INCLUDE_FILES(uuid/uuid.h HAVE_UUID_UUID_H)

CHECK_FUNCTION_EXISTS(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS(chroot HAVE_CHROOT)
CHECK_FUNCTION_EXISTS(pathconf HAVE_PATHCONF)
CHECK_FUNCTION_EXISTS(sendfile HAVE_SENDFILE)

CHECK_INCLUDE_FILES(sys/inotify.h HAVE_SYS_INOTIFY_H)
IF (HAVE_SYS_INOTIFY_H)
	CHECK_FUNCTION_EXISTS(inotify_init1 HAVE_INOTIFY_INIT1)
ENDIF(HAVE_SYS_INOTIFY_H)

CHECK_INCLUDE_FILES(zlib.h HAVE_ZLIB_H)
IF (HAVE_ZLIB_H)
	CHECK_LIBRARY_EXISTS(z deflate "" HAVE_LIBZ)
ENDIF(HAVE_ZLIB_H)

# {{{ sets
#if (DEFINED PREFIX)
#	set(PREFIX ${PREFIX} CACHE PATH "installation prefix")
#	set(CMAKE_INSTALL_PREFIX ${PREFIX})
#else()
#	set(PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "installation prefix")
#endif()

if (DEFINED SYSCONFDIR)
	set(SYSCONFDIR ${SYSCONFDIR} CACHE PATH "configuration directory")
else()
	set(SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc CACHE PATH "configuration directory")
endif()

if (DEFINED BOOST_INCLUDEDIR)
	set(BOOST_INCLUDEDIR ${BOOST_INCLUDEDIR} CACHE PATH "boost include directory")
else()
	set(BOOST_INCLUDEDIR "/usr/include/boost-1_39" CACHE PATH "boost include directory")
endif()

if (NOT CMAKE_BUILD_TYPE)
	#set(CMAKE_BUILD_TYPE "Release" CACHE STRING "e.g. Release or Debug, defaults to Debug")
	set(CMAKE_BUILD_TYPE "Release")
endif()
# }}}

#set(CMAKE_VERBOSE_MAKEFILE ON)

IF(WITH_LUA)
	pkg_check_modules(LUA REQUIRED lua>=5.1)
ENDIF(WITH_LUA)
IF(WITH_PCRE)
	pkg_check_modules(PCRE REQUIRED libpcre>=7.0)
ENDIF(WITH_PCRE)
IF(WITH_SSL)
	pkg_check_modules(GNUTLS REQUIRED gnutls>=2.0.0)
ENDIF(WITH_SSL)
IF(BUILD_TESTS)
	pkg_check_modules(CPPUNIT REQUIRED cppunit>=1.12)
ENDIF(BUILD_TESTS)

IF(BUILD_STATIC)
	# ...
ELSE(BUILD_STATIC)
	CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
ENDIF(BUILD_STATIC)

# ---------------------------------------------------------------------------
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/src/x0/sysconfig.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/x0/sysconfig.h)
ADD_DEFINITIONS(-DHAVE_SYSCONFIG_H)

# ---------------------------------------------------------------------------
message("")
message("Build type: ${CMAKE_BUILD_TYPE}")
message("Boost include directory: ${BOOST_INCLUDEDIR}")
message("LUA version: ${LUA_VERSION}")
message("")
