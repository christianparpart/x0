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
x0_plugin(accesslog)
x0_plugin(alias)
x0_plugin(cgi)
x0_plugin(compress)
x0_plugin(debug)
x0_plugin(dirlisting)
x0_plugin(indexfile)
x0_plugin(proxy)
x0_plugin(sendfile)
x0_plugin(ssl)
x0_plugin(userdir)
x0_plugin(vhost_basic)
x0_plugin(vhost_template)

x0_plugin(echo_example)
x0_plugin(hello_example)

#add_plugin(ssl)
#target_link_libraries(ssl ${GNUTLS_LIBRARIES})
