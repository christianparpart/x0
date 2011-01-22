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

set(x0_SRC ${x0_base_SRC} ${x0_io_SRC} ${x0_sql_SRC} ${x0_http_SRC})

add_library(x0 SHARED ${x0_SRC})
set_target_properties(x0 PROPERTIES LINK_FLAGS "${LDFLAGS} ${FLOW_LDFLAGS}")
target_link_libraries(x0 dl pthread ${SD_LIBRARIES} ${LIBS})
install(TARGETS x0 DESTINATION ${LIBDIR})
