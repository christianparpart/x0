cmake_minimum_required(VERSION 2.8)

project(x0)
set(PACKAGE_NAME "x0")
set(PACKAGE_VERSION "0.7.0-dev")
set(PACKAGE_HOMEPAGE_URL "http://xzero.io/")
set(PACKAGE_BUGTRACKER_URL "https://github.com/xzero/x0/issues/")

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
option(BUILD_TESTS "Build unit tests [default: off]" OFF)

option(ENABLE_EXAMPLES "Includes example plugins and programs" OFF)
option(ENABLE_WEBDAV "Includes WebDAV support (plugin)" OFF)
option(ENABLE_RRD "Includes RRD support (plugin)" OFF)
option(ENABLE_IMAGEABLE "Includes Image Resizer plugin [default: off]" OFF)

if(ENABLE_RRD)
	find_package(RRD)
endif()

if(ENABLE_IMAGEABLE)
	pkg_check_modules(IMAGEMAGICK REQUIRED MagickWand>=6.7.0)
endif(ENABLE_IMAGEABLE)

# feature flags
option(WITH_INOTIFY "Build with inotify support [default: on]" ON)
option(WITH_SSL "Builds with SSL support [default: on]" ON)
option(WITH_ACCEPT4 "Builds with support for accept4() system-call, if found [default: on]" ON)
option(WITH_TCP_DEFER_ACCEPT "Builds with support for TCP_DEFER_ACCEPT, if found [default: on]" ON)
option(WITH_MULTI_ACCEPT "Builds listener acceptor with support for repeative accept() calls  [default: off]" OFF)
option(WITH_OPPORTUNISTIC_WRITE "Builds with support for opportunistic write() calls to client sockets [default: off]." OFF)
option(WITH_TCMALLOC "Uses tcmalloc for memory allocation [default: off]." OFF)
#option(WITH_LIBAIO "Build with libaio support [default: on]" ON)

add_definitions(-Wall -Wno-variadic-macros)

# we need the following definitions in order to get some special
# OS-level features like posix_fadvise() or readahead().
add_definitions(-DXOPEN_SOURCE=600)
add_definitions(-DGNU_SOURCE)

# enforce 64bit i/o operations, even on 32bit platforms
add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES)

CHECK_INCLUDE_FILES(fcntl.h HAVE_FCNTL_H)
CHECK_INCLUDE_FILES(netdb.h HAVE_NETDB_H)
CHECK_INCLUDE_FILES(gtest/gtest.h HAVE_GTEST_GTEST_H)

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
CHECK_INCLUDE_FILES(ldap.h HAVE_LDAP_H)

CHECK_FUNCTION_EXISTS(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS(chroot HAVE_CHROOT)
CHECK_FUNCTION_EXISTS(pathconf HAVE_PATHCONF)
CHECK_FUNCTION_EXISTS(accept4 HAVE_ACCEPT4)

if(WITH_INOTIFY)
	CHECK_INCLUDE_FILES(sys/inotify.h HAVE_SYS_INOTIFY_H)
	if(HAVE_SYS_INOTIFY_H)
		CHECK_FUNCTION_EXISTS(inotify_init1 HAVE_INOTIFY_INIT1)
	endif(HAVE_SYS_INOTIFY_H)
endif(WITH_INOTIFY)

if(WITH_TCMALLOC)
	CHECK_INCLUDE_FILES(google/tcmalloc.h HAVE_GOOGLE_TCMALLOC_H)
	pkg_check_modules(TCMALLOC REQUIRED libtcmalloc>=1.8.3)
endif(WITH_TCMALLOC)

CHECK_INCLUDE_FILES(zlib.h HAVE_ZLIB_H)
if(HAVE_ZLIB_H)
	CHECK_LIBRARY_EXISTS(z deflate "" HAVE_LIBZ)
	set(ZLIB_LIBRARIES z)
endif(HAVE_ZLIB_H)

CHECK_INCLUDE_FILES(bzlib.h HAVE_BZLIB_H)
if(HAVE_BZLIB_H)
	CHECK_LIBRARY_EXISTS(bz2 BZ2_bzCompressInit "" HAVE_LIBBZ2)
	set(BZIP2_LIBRARIES bz2)
endif(HAVE_BZLIB_H)

# ---------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/support/sd-daemon)

# LLVM
find_package(LLVM)
include_directories(${LLVM_INCLUDE_DIR})
link_directories(${LLVM_LIBRARY_DIR})
set(LIBS ${LIBS} ${LLVM_LIBRARIES})
add_definitions(${LLVM_CPPFLAGS})

string(COMPARE EQUAL "${LLVM_VERSION}" "3.0" LLVM_FOUND_V_3_0)
string(COMPARE EQUAL "${LLVM_VERSION}" "3.1" LLVM_FOUND_V_3_1)
if(${LLVM_FOUND_V_3_1})
	message(STATUS "Your LLVM version is supported: ${LLVM_VERSION}")
elseif(${LLVM_FOUND_V_3_0})
	message(STATUS "Your LLVM version is supported: ${LLVM_VERSION}")
else()
	message(FATAL_ERROR "LLVM version 3.0 or 3.1 is required (You have ${LLVM_VERSION}).\nYou can pass -DLLVM_CONFIG_EXECUTABLE=/path/to/llvm-config for custom paths/names to llvm-config.")
endif()

# PCRE
option(WITH_PCRE "With PCRE support [default: off]" ON)
if(WITH_PCRE)
	pkg_check_modules(PCRE REQUIRED libpcre>=7.0)
endif(WITH_PCRE)

# ev
find_package(EV)
include_directories(${EV_INCLUDE_DIR})
link_directories(${EV_LIBRARY_DIR})
add_definitions(${EV_CPPFLAGS})

# mySQL
option(WITH_MYSQL "With mySQL support [default: on]" ON)
if(WITH_MYSQL)
	find_package(MYSQL)
	include_directories(${MYSQL_INCLUDE_DIR})
	set(LIBS ${LIBS} ${MYSQL_LDFLAGS})
	add_definitions(${MYSQL_CFLAGS})
endif()

# valgrind
set(VALGRIND OFF CACHE BOOL "generate binary for use with valgrind")

# systemd support (implemented in support/sd-daemon/)
set(SD_LIBRARIES sd-daemon)

# {{{ path prefixes
# ---------------------------------------------------------------------------
if (DEFINED CMAKE_INSTALL_PREFIX)
	set(SYSCONFDIR ${CMAKE_INSTALL_PREFIX}/etc CACHE PATH "x0d configuration directory")
	set(INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include CACHE PATH "x0 include directory")
	set(LIBDIR ${CMAKE_INSTALL_PREFIX}/lib CACHE PATH "x0 library directory")
	set(LOGDIR ${CMAKE_INSTALL_PREFIX}/var/log CACHE PATH "log file directory")
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

if (DEFINED LOGDIR)
	set(LOGDIR ${LOGDIR} CACHE PATH "log file directory")
else()
	set(LOGDIR ${CMAKE_INSTALL_PREFIX}/var/log CACHE PATH "log file directory")
endif()

set(PLUGINDIR "${LIBDIR}/x0" CACHE PATH "path to x0 plugins")
# ---------------------------------------------------------------------------
# }}}

#set(CMAKE_VERBOSE_MAKEFILE ON)

#pkg_check_modules(LUA REQUIRED lua>=5.1)
#message(STATUS "LUA include directory: " ${LUA_INCLUDE_DIRS})
#message(STATUS "LUA libraries: " ${LUA_LIBRARIES})

if(WITH_SSL)
	pkg_check_modules(GNUTLS REQUIRED gnutls>=2.0.0)
endif(WITH_SSL)

if(BUILD_TESTS)
	pkg_check_modules(CPPUNIT REQUIRED cppunit>=1.12)
	find_package(FCGI)
endif(BUILD_TESTS)

#if(BUILD_STATIC)
	# ...
#else(BUILD_STATIC)
	CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)
#endif(BUILD_STATIC)

# ---------------------------------------------------------------------------
macro(x0_plugin_simple ModuleName)
	add_library(${ModuleName} MODULE ${ModuleName}.cpp)
	set_target_properties(${ModuleName} PROPERTIES PREFIX "")
	install(TARGETS ${ModuleName} DESTINATION ${PLUGINDIR})
endmacro()

macro(x0_plugin ModuleName SrcFiles)
	add_library(${ModuleName} MODULE ${SrcFiles})
	set_target_properties(${ModuleName} PROPERTIES PREFIX "")
	install(TARGETS ${ModuleName} DESTINATION ${PLUGINDIR})
endmacro()

# ---------------------------------------------------------------------------
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/include/x0/sysconfig.h.cmake
	${CMAKE_CURRENT_BINARY_DIR}/include/x0/sysconfig.h)

# ---------------------------------------------------------------------------
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/XzeroBase.pc.cmake
	${CMAKE_CURRENT_BINARY_DIR}/XzeroBase.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/XzeroBase.pc DESTINATION ${LIBDIR}/pkgconfig)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/XzeroFlow.pc.cmake
	${CMAKE_CURRENT_BINARY_DIR}/XzeroFlow.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/XzeroFlow.pc DESTINATION ${LIBDIR}/pkgconfig)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/XzeroHttp.pc.cmake
	${CMAKE_CURRENT_BINARY_DIR}/XzeroHttp.pc)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/XzeroHttp.pc DESTINATION ${LIBDIR}/pkgconfig)

# ---------------------------------------------------------------------------
add_definitions(-DHAVE_SYSCONFIG_H)

add_subdirectory(docs)
add_subdirectory(support)
add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(plugins)
add_subdirectory(tests)
add_subdirectory(examples)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
	DESTINATION ${INCLUDEDIR}
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "*.cc")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/
	DESTINATION ${INCLUDEDIR}
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "*.cc")

add_custom_target(ctags
	COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q ${CMAKE_SOURCE_DIR}/lib ${CMAKE_SOURCE_DIR}/include
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
