cmake_minimum_required(VERSION 2.8)

project(libbase)
set(LIBBASE_VERSION "0.11.0-dev")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(FindPkgConfig)
include(FindDoxygen)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
include(CheckVariableExists)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckCSourceCompiles)
include(CMakeDetermineCCompiler)

option(ENABLE_TESTS "Build unit tests [default: off]" OFF)
option(ENABLE_EXAMPLES "Includes example programs [default: on]" ON)

# Xzero Base: platform feature flags
option(ENABLE_PCRE "With PCRE support [default: off]" ON)
option(ENABLE_ACCEPT4 "Builds with support for accept4() system-call, if found [default: on]" ON)
option(ENABLE_MULTI_ACCEPT "Builds listener acceptor with support for repeative accept() calls  [default: on]" ON)
option(ENABLE_TCP_DEFER_ACCEPT "Builds with support for TCP_DEFER_ACCEPT, if found [default: on]" ON)
option(ENABLE_MYSQL "With mySQL support [default: on]" ON)

# Xzero Base: implementation features
option(BASE_QUEUE_LOCKFREE "Queue<T> is using a lock-free algorithm [default: on]" ON)

# XXX do not use -std=c++11 here, as it's not supported on GCC 4.6 (default on Ubuntu 12.04)
set(CMAKE_CXX_FLAGS "-std=c++0x")

add_definitions(-Wall -Wno-variadic-macros)

# temporary workaround for: http://gcc.gnu.org/bugzilla/show_bug.cgi?id=56627
add_definitions(-Wno-mismatched-tags)

# we need the following definitions in order to get some special
# OS-level features like posix_fadvise() or readahead().
add_definitions(-DXOPEN_SOURCE=600)
add_definitions(-D_GNU_SOURCE)

# ISO C99: explicitely request format specifiers
add_definitions(-D__STDC_FORMAT_MACROS)

# enforce 64bit i/o operations, even on 32bit platforms
add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES)

CHECK_INCLUDE_FILES(fcntl.h HAVE_FCNTL_H)
CHECK_INCLUDE_FILES(netdb.h HAVE_NETDB_H)

CHECK_INCLUDE_FILES(sys/uio.h HAVE_SYS_UIO_H)
CHECK_INCLUDE_FILES(sys/sendfile.h HAVE_SYS_SENDFILE_H)
CHECK_FUNCTION_EXISTS(sendfile HAVE_SENDFILE)
CHECK_FUNCTION_EXISTS(posix_fadvise HAVE_POSIX_FADVISE)
CHECK_FUNCTION_EXISTS(readahead HAVE_READAHEAD)
CHECK_FUNCTION_EXISTS(pipe2 HAVE_PIPE2)
CHECK_FUNCTION_EXISTS(splice HAVE_SPLICE)
CHECK_FUNCTION_EXISTS(tee HAVE_TEE)

CHECK_INCLUDE_FILES(sys/resource.h HAVE_SYS_RESOURCE_H)
CHECK_INCLUDE_FILES(sys/limits.h HAVE_SYS_LIMITS_H)
CHECK_INCLUDE_FILES(sys/mman.h HAVE_SYS_MMAN_H)
CHECK_INCLUDE_FILES(syslog.h HAVE_SYSLOG_H)
CHECK_INCLUDE_FILES(pwd.h HAVE_PWD_H)
CHECK_INCLUDE_FILES(uuid/uuid.h HAVE_UUID_UUID_H)
CHECK_INCLUDE_FILES(sys/utsname.h HAVE_SYS_UTSNAME_H)
CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)

CHECK_FUNCTION_EXISTS(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS(pathconf HAVE_PATHCONF)
CHECK_FUNCTION_EXISTS(accept4 HAVE_ACCEPT4)
CHECK_LIBRARY_EXISTS(pthread pthread_setname_np "" HAVE_PTHREAD_SETNAME_NP)
CHECK_LIBRARY_EXISTS(pthread pthread_setaffinity_np "" HAVE_PTHREAD_SETAFFINITY_NP)

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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/sd-daemon)

# PCRE
if(ENABLE_PCRE)
  pkg_check_modules(PCRE REQUIRED libpcre>=7.0)
endif()

# ev
find_package(EV)
include_directories(${EV_INCLUDE_DIR})
link_directories(${EV_LIBRARY_DIR})
add_definitions(${EV_CPPFLAGS})

# mySQL
if(ENABLE_MYSQL)
  find_package(MYSQL)
  include_directories(${MYSQL_INCLUDE_DIR})
  set(LIBS ${LIBS} ${MYSQL_LDFLAGS})
  add_definitions(${MYSQL_CFLAGS})
endif()

pkg_check_modules(SD libsystemd-daemon)

if(ENABLE_TESTS)
  CHECK_INCLUDE_FILES(gtest/gtest.h HAVE_GTEST_GTEST_H)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/base/sysconfig.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/include/base/sysconfig.h)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libbase.pc.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libbase.pc)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libbase.pc 
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

add_definitions(-DHAVE_SYSCONFIG_H)

add_subdirectory(src)
add_subdirectory(include)
add_subdirectory(tests)
add_subdirectory(examples)
