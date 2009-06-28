cmake_minimum_required(VERSION 2.6)

project(x0)
set(PACKAGE_NAME "x0")
set(PACKAGE_VERSION "0.1.0")
set(PACKAGE_HOMEPAGE_URL "http://code.ninchens.net/projects/x0")
set(PACKAGE_BUGTRACKER_URL "http://code.ninchens.net/projects/x0/issues")

INCLUDE(FindPkgConfig)
INCLUDE(FindDoxygen)

add_subdirectory(src)
add_subdirectory(tests)

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

pkg_check_modules(LUA REQUIRED lua>=5.1)
pkg_check_modules(CPPUNIT REQUIRED cppunit>=1.12)

# ---------------------------------------------------------------------------
message("")
message("Build type: ${CMAKE_BUILD_TYPE}")
message("Boost include directory: ${BOOST_INCLUDEDIR}")
message("LUA version: ${LUA_VERSION}")
message("")
