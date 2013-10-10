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

set(ssl_SRC
	SslDriver.cpp
	SslSocket.cpp
	SslContext.cpp
	ssl.cpp)

x0_plugin(ssl "${ssl_SRC}")
target_link_libraries(ssl ${GNUTLS_LIBRARIES} gcrypt)
