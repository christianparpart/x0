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
macro(add_plugin ModuleName)
	add_library(${ModuleName} MODULE ${ModuleName}.cpp)
	set_target_properties(${ModuleName} PROPERTIES PREFIX "")
	install(TARGETS ${ModuleName} DESTINATION lib/x0)
endmacro()

add_plugin(accesslog)
#add_plugin(alias)
#add_plugin(cgi)
#add_plugin(debug)
add_plugin(dirlisting)
#add_plugin(indexfile)
add_plugin(sendfile)
add_plugin(userdir)
add_plugin(vhost_basic)
#add_plugin(vhost_template)

#add_plugin(ssl)
#target_link_libraries(ssl ${GNUTLS_LIBRARIES})
