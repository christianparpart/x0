cmake_minimum_required(VERSION 2.8)

project(libflow)
set(LIBFLOW_NAME "libflow")
set(LIBFLOW_VERSION "0.11.0-dev")
set(LIBFLOW_HOMEPAGE_URL "http://xzero.io/")
set(LIBFLOW_BUGTRACKER_URL "https://github.com/xzero/libflow/issues/")

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
option(FLOW_ENABLE_EXAMPLES "Builds example programs [default: on]" ON)
option(FLOW_ENABLE_FLOW_TOOL "Builds flow-tool [default: on]" ON)

# XXX do not use -std=c++11 here, as it's not supported on GCC 4.6 (default on Ubuntu 12.04)
set(CMAKE_CXX_FLAGS "-std=c++0x")

include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# libbase integration
if(EXISTS ${X0D_TOPLEVEL_3RDPARTY_DIR}/libbase)
  set(BASE_LIBRARIES base)
  message("libflow: use libbase from ${X0D_TOPLEVEL_3RDPARTY_DIR}/libbase")
elseif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbase)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbase)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/libbase/include)
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/3rdparty/libbase/include)
  set(BASE_LIBRARIES base)
  message("libbase: use from 3rdparty/libbase")
else()
  pkg_check_modules(BASE REQUIRED libbase)
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

if(ENABLE_TESTS)
  CHECK_INCLUDE_FILES(gtest/gtest.h HAVE_GTEST_GTEST_H)
endif()

add_definitions(-DHAVE_SYSCONFIG_H)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/flow/sysconfig.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/include/flow/sysconfig.h)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/libflow.pc.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/libflow.pc)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libflow.pc
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)
add_subdirectory(docs)

if(FLOW_ENABLE_FLOW_TOOL)
  add_subdirectory(flow-tool)
endif()
