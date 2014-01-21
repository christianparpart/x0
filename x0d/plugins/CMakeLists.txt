add_definitions(
  -pthread
  -std=c++0x
  -DPACKAGE_NAME="${PACKAGE_NAME}"
  -DPACKAGE_VERSION="${PACKAGE_VERSION}"
  -DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
  -DSYSCONFDIR="${SYSCONFDIR}"
  -DVERSION="${PACKAGE_VERSION}")

# ---------------------------------------------------------------------------
if(ENABLE_PLUGINS)
x0_plugin_simple(accesslog)
x0_plugin_simple(browser)
x0_plugin_simple(cgi)
x0_plugin_simple(compress)
x0_plugin_simple(debug)
x0_plugin_simple(expire)
x0_plugin_simple(dirlisting)
x0_plugin_simple(userdir)

x0_plugin_simple(auth)
if(HAVE_SECURITY_PAM_APPL_H)
  # optionally link against libpam if its header file was found and 
  # we are to include PAM support
  target_link_libraries(auth pam)
endif(HAVE_SECURITY_PAM_APPL_H)

if(ENABLE_PLUGIN_RRD)
  x0_plugin_simple(rrdfile)
  set_target_properties(rrdfile PROPERTIES COMPILE_FLAGS "${CFLAGS} -I${RRD_INCLUDE_PATH}")
  target_link_libraries(rrdfile ${RRD_LIBRARY_PATH})
endif()


# XXX Disabled until Ubuntu 14.04 comes out, as it (should) support a newer
# XXX cmake version that actually supports this way of linking (or tell me how to
# XXX do it on the older one)
#
#if(ENABLE_PLUGIN_IMAGEABLE)
#  x0_plugin_simple(imageable)
#  set_target_properties(imageable PROPERTIES COMPILE_FLAGS ${IMAGEMAGICK_CFLAGS})
#  target_include_directories(imageable PUBLIC ${IMAGEMAGICK_INCLUDE_DIRS})
#  target_link_libraries(imageable ${IMAGEMAGICK_LIBRARIES})
#endif()

x0_plugin_simple(status)

if(ENABLE_PLUGIN_WEBDAV)
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

endif(ENABLE_PLUGINS)
