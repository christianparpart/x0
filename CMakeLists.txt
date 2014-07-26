cmake_minimum_required(VERSION 2.8)

project(x0)
set(PACKAGE_NAME "x0")
set(PACKAGE_VERSION "0.11.0-dev")
set(PACKAGE_HOMEPAGE_URL "http://xzero.io/")
set(PACKAGE_BUGTRACKER_URL "https://github.com/xzero/x0/issues/")

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
option(ENABLE_EXAMPLES "Includes example plugins and programs [default: on]" ON)

option(ENABLE_O_TMPFILE "Try to open temporary files with O_TMPFILE flag before falling back to standard behaviour [default: on]" ON)

# x0d plugins
option(ENABLE_PLUGINS "Compile x0d plugins [default: on]" ON)
option(ENABLE_PLUGIN_AUTH_PAM "Build with 'auth' plugin with PAM authentication support [default: on]" ON)
option(ENABLE_PLUGIN_IMAGEABLE "Includes Image Resizer plugin [default: off]" OFF)
option(ENABLE_PLUGIN_RRD "Includes RRD support (plugin) [default: off]" OFF)
option(ENABLE_PLUGIN_SSL "Builds with SSL support [default: on]" ON)
option(ENABLE_PLUGIN_DIRECTOR "Builds director plugin (http/fastcgi proxy+loadbalancer) [default: on]" ON)
option(ENABLE_PLUGIN_WEBDAV "Includes WebDAV support (plugin) [default: off]" OFF)
option(ENABLE_PLUGIN_DEBUG "Includes x0d debugging support (plugin) [default: off]" OFF)

# x0d plugin feature flags
option(ENABLE_DIRECTOR_CACHE "Enables response cache accelerator [default: on]" ON)

option(ENABLE_MANPAGES "Build manual pages [default: on]" ON)
option(ENABLE_TCMALLOC "Uses tcmalloc for memory allocation [default: off]." OFF)

if(ENABLE_PLUGIN_AUTH_PAM)
  CHECK_INCLUDE_FILES(security/pam_appl.h HAVE_SECURITY_PAM_APPL_H)
endif()

if(ENABLE_PLUGIN_IMAGEABLE)
  pkg_check_modules(IMAGEMAGICK REQUIRED MagickWand>=6.6.0)
endif()

if(ENABLE_PLUGIN_RRD)
  find_package(RRD)
endif()

if(ENABLE_PLUGIN_SSL)
  pkg_check_modules(GNUTLS REQUIRED gnutls>=2.0.0)
endif()

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
CHECK_INCLUDE_FILES(dlfcn.h HAVE_DLFCN_H)

CHECK_FUNCTION_EXISTS(fork HAVE_FORK)
CHECK_FUNCTION_EXISTS(chroot HAVE_CHROOT)
CHECK_FUNCTION_EXISTS(pathconf HAVE_PATHCONF)
CHECK_FUNCTION_EXISTS(accept4 HAVE_ACCEPT4)
CHECK_LIBRARY_EXISTS(pthread pthread_setname_np "" HAVE_PTHREAD_SETNAME_NP)
CHECK_LIBRARY_EXISTS(pthread pthread_setaffinity_np "" HAVE_PTHREAD_SETAFFINITY_NP)

if(ENABLE_TCMALLOC)
  CHECK_INCLUDE_FILES(google/tcmalloc.h HAVE_GOOGLE_TCMALLOC_H)
  # pkg_check_modules(TCMALLOC REQUIRED libtcmalloc>=1.8.3)
  set(TCMALLOC_LIBRARIES tcmalloc)
endif()

# TODO dynamic check for tbb
# tbb is required for director plugin and for base's TimeoutScheduler (needed?)
# However, onn Ubuntu 14.04, the standard clang++ causes this test to fail.
if(NOT ("${CMAKE_CXX_COMPILER}" MATCHES "clang"))
  if(ENABLE_PLUGIN_DIRECTOR)
    CHECK_INCLUDE_FILE_CXX(tbb/tbb.h HAVE_TBB_TBB_H)
    if(NOT HAVE_TBB_TBB_H)
      message(FATAL_ERROR "Can't find <tbb/tbb.h> include file.\nPlease install Threading Building Blocks (e.g. libtbb-dev).")
    endif()
  endif()
endif()
set(TBB_LIBRARIES tbb)

# {{{ 3rdparty integration
set(X0D_TOPLEVEL_3RDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty")

# libbase integration
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbase)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbase/include)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/3rdparty/libbase/include)
  set(BASE_LIBRARIES base)
  message("libbase: use from 3rdparty/libbase")
else()
  pkg_check_modules(BASE REQUIRED "libbase")
  add_definitions(${BASE_CFLAGS})
  add_definitions(${BASE_CPPFLAGS})
  include_directories(${BASE_INCLUDE_DIR})
  include_directories(${BASE_INCLUDE_DIRS})
  link_directories(${BASE_LIBRARY_DIRS})

  message("libbase include dirs: ${BASE_INCLUDE_DIRS}")
  message("libbase cflags: ${BASE_CFLAGS}")
  message("libbase ldflags: ${BASE_LDFLAGS}")
  message("libbase libs: ${BASE_LIBRARIES}")
  message("libbase libdir: ${BASE_LIBRARY_DIRS}")
endif()

# libxzero integration
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libxzero)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libxzero/include)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/3rdparty/libxzero/include)
  set(XZERO_LIBRARIES xzero)
  message("libxzero: use from 3rdparty/libxzero")
else()
  pkg_check_modules(XZERO REQUIRED "libxzero")
  add_definitions(${XZERO_CFLAGS})
  add_definitions(${XZERO_CPPFLAGS})
  include_directories(${XZERO_INCLUDE_DIR})
  include_directories(${XZERO_INCLUDE_DIRS})
  link_directories(${XZERO_LIBRARY_DIRS})

  message("libxzero include dirs: ${XZERO_INCLUDE_DIRS}")
  message("libxzero cflags: ${XZERO_CFLAGS}")
  message("libxzero ldflags: ${XZERO_LDFLAGS}")
  message("libxzero libs: ${XZERO_LIBRARIES}")
  message("libxzero libdir: ${XZERO_LIBRARY_DIRS}")
endif()

# libflow integration
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libflow)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libflow/include)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/3rdparty/libflow/include)
  set(FLOW_LIBRARIES flow)
  message("libflow: use from 3rdparty/libflow")
else()
  pkg_check_modules(FLOW REQUIRED "libflow")
  add_definitions(${FLOW_CFLAGS})
  add_definitions(${FLOW_CPPFLAGS})
  include_directories(${FLOW_INCLUDE_DIR})
  include_directories(${FLOW_INCLUDE_DIRS})
  link_directories(${FLOW_LIBRARY_DIRS})

  message("libflow include dirs: ${FLOW_INCLUDE_DIRS}")
  message("libflow cflags: ${FLOW_CFLAGS}")
  message("libflow ldflags: ${FLOW_LDFLAGS}")
  message("libflow libs: ${FLOW_LIBRARIES}")
  message("libflow libdir: ${FLOW_LIBRARY_DIRS}")
endif()
# }}}

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

pkg_check_modules(SD libsystemd-daemon)

if(ENABLE_TESTS)
  find_package(FCGI)
  CHECK_INCLUDE_FILES(gtest/gtest.h HAVE_GTEST_GTEST_H)
endif()

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

set(PLUGINDIR "${LIBDIR}/x0d" CACHE PATH "path to x0d plugins")
# ---------------------------------------------------------------------------
# }}}

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

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/x0d/sysconfig.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/include/x0d/sysconfig.h)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/x0d.pc.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/x0d.pc)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/x0d.pc
    DESTINATION ${LIBDIR}/pkgconfig)

add_custom_target(ctags
    COMMAND ctags -R --c++-kinds=+p --fields=+iaS --extra=+q ${CMAKE_SOURCE_DIR}/lib ${CMAKE_SOURCE_DIR}/include
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_custom_target(cscope
    COMMAND echo -n > ${CMAKE_SOURCE_DIR}/cscope.files
            && find ${CMAKE_SOURCE_DIR}/lib -name '*.h' -o -name '*.cc' -o -name '*.cpp' >> ${CMAKE_SOURCE_DIR}/cscope.files
            && find ${CMAKE_SOURCE_DIR}/include -name '*.h' -o -name '*.cc' -o -name '*.hpp' >> ${CMAKE_SOURCE_DIR}/cscope.files
            && cscope -q -R -b -i cscope.files
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(release
    COMMAND ${CMAKE_SOURCE_DIR}/contrib/ppa-release.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_subdirectory(docs)
add_subdirectory(3rdparty)
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)
