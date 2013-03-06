include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
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

# ---------------------------------------------------------------------------
x0_plugin_simple(accesslog)
x0_plugin_simple(auth)
x0_plugin_simple(browser)
x0_plugin_simple(cgi)
x0_plugin_simple(compress)
x0_plugin_simple(debug)
x0_plugin_simple(expire)
x0_plugin_simple(dirlisting)
x0_plugin_simple(userdir)
x0_plugin_simple(vhost)

if(ENABLE_RRD)
	x0_plugin_simple(rrdfile)
	set_target_properties(rrdfile PROPERTIES COMPILE_FLAGS "${CFLAGS} -I${RRD_INCLUDE_PATH}")
	target_link_libraries(rrdfile ${RRD_LIBRARY_PATH})
endif()

if(ENABLE_IMAGEABLE)
	x0_plugin_simple(imageable)
	set_target_properties(imageable PROPERTIES COMPILE_FLAGS "${CFLAGS} ${IMAGEMAGICK_CFLAGS_OTHER} -I${IMAGEMAGICK_INCLUDE_DIRS}")
	target_link_libraries(imageable ${IMAGEMAGICK_LIBRARIES})
endif()

x0_plugin_simple(status)

if(ENABLE_WEBDAV)
	x0_plugin_simple(webdav)
endif()

if(ENABLE_EXAMPLES)
	x0_plugin_simple(echo_example)
	x0_plugin_simple(hello_example)
	x0_plugin_simple(filter_example)
endif()

# ---------------------------------------------------------------------------
# the more complex plugins:

add_subdirectory(ssl)
add_subdirectory(director)
