add_definitions(
  -pthread
  -DPACKAGE_NAME="${PACKAGE_NAME}"
  -DPACKAGE_VERSION="${PACKAGE_VERSION}"
  -DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
  -DSYSCONFDIR="${SYSCONFDIR}"
  -DLOGDIR="${LOGDIR}"
  -DPLUGINDIR="${PLUGINDIR}"
  -DVERSION="${PACKAGE_VERSION}"
)

set(x0d_SRC
  XzeroCore.cpp
  XzeroDaemon.cpp
  XzeroEventHandler.cpp
  XzeroPlugin.cpp
  x0d.cpp
)

add_executable(x0d ${x0d_SRC})
target_link_libraries(x0d ${BASE_LIBRARIES} ${XZERO_LIBRARIES} ${FLOW_LIBRARIES} ${SD_LIBRARIES} ${TCMALLOC_LIBRARIES})
set_target_properties(x0d PROPERTIES COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_X0D=1")
#set_target_properties(x0d PROPERTIES LINK_FLAGS "${BASE_LDFLAGS}")
install(TARGETS x0d RUNTIME DESTINATION sbin)
install(FILES x0d.conf-dist DESTINATION ${SYSCONFDIR})

add_subdirectory(plugins)
