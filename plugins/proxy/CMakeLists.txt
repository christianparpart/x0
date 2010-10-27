include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../..)
include_directories(${BOOST_INCLUDEDIR})

add_definitions(
	-Dlinux=1
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-fvisibility=hidden
	-DPACKAGE_NAME="${PACKAGE_NAME}"
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DVERSION="${PACKAGE_VERSION}")

# ---------------------------------------------------------------------------

set(proxy_API
	ProxyOrigin.h
	ProxyConnection.h)

set(proxy_SRC
	ProxyOrigin.cpp
	ProxyConnection.cpp
	proxy.cpp)

x0_plugin(proxy "${proxy_SRC}")
#target_link_libraries(proxy ${GNUTLS_LIBRARIES})
