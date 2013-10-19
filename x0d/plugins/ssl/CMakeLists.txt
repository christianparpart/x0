if(ENABLE_PLUGIN_SSL)
  add_definitions(
    -pthread
    -std=c++0x
    -fvisibility=hidden
    -DPACKAGE_NAME="${PACKAGE_NAME}"
    -DPACKAGE_VERSION="${PACKAGE_VERSION}"
    -DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
    -DSYSCONFDIR="${SYSCONFDIR}"
    -DVERSION="${PACKAGE_VERSION}")

  set(ssl_SRC
    SslDriver.cpp
    SslSocket.cpp
    SslContext.cpp
    ssl.cpp)

  x0_plugin(ssl "${ssl_SRC}")
  target_link_libraries(ssl ${GNUTLS_LIBRARIES} gcrypt)
endif()
