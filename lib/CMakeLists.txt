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

file(GLOB_RECURSE x0_SRC "*.cpp")

add_library(x0 SHARED ${x0_SRC})
set_target_properties(x0 PROPERTIES LINK_FLAGS "${LDFLAGS} ${FLOW_LDFLAGS}")
target_link_libraries(x0 dl pthread ${SD_LIBRARIES} ${LIBS})
install(TARGETS x0 DESTINATION ${LIBDIR})
