# Xzero C API
if(ENABLE_CAPI)

add_library(x0capi SHARED
  http.cpp
  inifile.cpp
)

target_link_libraries(x0capi XzeroHttp XzeroBase)
set_target_properties(x0capi PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS x0capi DESTINATION ${LIBDIR})

endif(ENABLE_CAPI)
