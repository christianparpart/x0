add_definitions(
	-Dlinux=1
	-Wall -Wno-deprecated
	-pthread
	-std=c++0x
	-stdlib=libc++
	-DPACKAGE_NAME="${PACKAGE_NAME}"
	-DPACKAGE_VERSION="${PACKAGE_VERSION}"
	-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
	-DSYSCONFDIR="${SYSCONFDIR}"
	-DPLUGINDIR="${PLUGINDIR}"
	-DLOGDIR="${LOGDIR}"
	-DVERSION="${PACKAGE_VERSION}")

add_definitions(-fvisibility=hidden -fvisibility-inlines-hidden -DBUILD_X0=1)

# XzeroBase
file(GLOB x0_base_SRC "base/*.cpp")
file(GLOB x0_io_SRC "io/*.cpp")
if(ENABLE_MYSQL)
	file(GLOB x0_sql_SRC "sql/*.cpp")
	add_definitions(${MYSQL_CFLAGS})
endif()
add_library(XzeroBase SHARED ${x0_base_SRC} ${x0_io_SRC} ${x0_sql_SRC})

if(ENABLE_MYSQL)
	target_link_libraries(XzeroBase ${MYSQL_LDFLAGS})
endif()

target_link_libraries(XzeroBase dl pthread
	${SD_LIBRARIES}
	${TBB_LIBRARIES}
	${ZLIB_LIBRARIES} ${BZIP2_LIBRARIES} ${PCRE_LIBRARIES}
	${EV_LIBRARIES})
set_target_properties(XzeroBase PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS XzeroBase DESTINATION ${LIBDIR})

# XzeroHttp
file(GLOB x0_http_SRC "http/*.cpp")
add_library(XzeroHttp SHARED ${x0_http_SRC})
target_link_libraries(XzeroHttp XzeroBase)
set_target_properties(XzeroHttp PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS XzeroHttp DESTINATION ${LIBDIR})

if(ENABLE_XZERO_FLOW)
	# XzeroFlow
	file(GLOB x0_flow_SRC "flow/*.cpp")
	add_library(XzeroFlow SHARED ${x0_flow_SRC})
	set_target_properties(XzeroFlow PROPERTIES LINK_FLAGS "${LDFLAGS} ${LLVM_LDFLAGS}")
	target_link_libraries(XzeroFlow LINK_PRIVATE ${LLVM_LIBRARIES} XzeroBase dl pthread)
	set_target_properties(XzeroFlow PROPERTIES VERSION ${PACKAGE_VERSION})
	install(TARGETS XzeroFlow DESTINATION ${LIBDIR})
endif()

# Xzero C API
add_library(x0capi SHARED capi.cpp)
set_target_properties(x0capi PROPERTIES LINK_FLAGS "${LDFLAGS} ${LLVM_LDFLAGS}")
target_link_libraries(x0capi XzeroHttp XzeroBase)
set_target_properties(x0capi PROPERTIES VERSION ${PACKAGE_VERSION})
install(TARGETS x0capi DESTINATION ${LIBDIR})
