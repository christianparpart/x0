if(ENABLE_EXAMPLES)
  add_definitions(
    -pthread
    -DPACKAGE_NAME="${PACKAGE_NAME}"
    -DPACKAGE_VERSION="${PACKAGE_VERSION}"
    -DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
    -DSYSCONFDIR="${SYSCONFDIR}"
    -DVERSION="${PACKAGE_VERSION}")

  #not supported on GCC 4.6 (Ubuntu 12.04?)
  #set(CMAKE_C_FLAGS "-std=c11")

  set(CMAKE_CXX_FLAGS "-std=c++0x")

  # FIXME: temporarily disabled, due to major restructurings in API composition
  add_executable(app1 app1.cpp)
  target_link_libraries(app1 XzeroBase XzeroHttp)

  add_executable(capi-app1 capi-app1.c)
  target_link_libraries(capi-app1 XzeroBase x0capi)

  add_executable(capi-app2 capi-app2.c)
  target_link_libraries(capi-app2 XzeroBase x0capi)

  add_executable(capi-post-data capi-post-data.c)
  target_link_libraries(capi-post-data XzeroBase x0capi)

  add_executable(tcp-echo-server tcp-echo-server.cpp)
  target_link_libraries(tcp-echo-server XzeroBase)

  add_executable(tcp-echo-server-splice tcp-echo-server-splice.cpp)
  target_link_libraries(tcp-echo-server-splice XzeroBase)
endif()
