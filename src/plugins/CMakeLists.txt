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
#x0_plugin_simple(cgi)
x0_plugin_simple(fastcgi)
x0_plugin_simple(compress)
#x0_plugin_simple(debug)
x0_plugin_simple(dirlisting)
x0_plugin_simple(userdir)
#x0_plugin_simple(vhost_basic)
#x0_plugin_simple(vhost_template)

x0_plugin_simple(echo_example)
x0_plugin_simple(hello_example)
#x0_plugin_simple(filter_example)

# ---------------------------------------------------------------------------
# the more complex plugins:

add_subdirectory(proxy)
add_subdirectory(ssl)
