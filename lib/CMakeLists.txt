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

file(GLOB x0_base_SRC "base/*.cpp")
file(GLOB x0_io_SRC "io/*.cpp")
file(GLOB x0_http_SRC "http/*.cpp")

if(WITH_MYSQL)
	file(GLOB x0_sql_SRC "sql/*.cpp")
endif()

# XzeroBase
add_library(XzeroBase SHARED ${x0_base_SRC} ${x0_io_SRC} ${x0_sql_SRC})
target_link_libraries(XzeroBase dl pthread
	${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES}
	${EV_LIBRARIES} ${MYSQL_LIBRARIES})
install(TARGETS XzeroBase DESTINATION ${LIBDIR})

# XzeroHttp
add_library(XzeroHttp SHARED ${x0_http_SRC})
set_target_properties(XzeroHttp PROPERTIES LINK_FLAGS "${LDFLAGS} ${FLOW_LDFLAGS}")
target_link_libraries(XzeroHttp XzeroBase ${SD_LIBRARIES} ${FLOW_LIBRARIES})
install(TARGETS XzeroHttp DESTINATION ${LIBDIR})
