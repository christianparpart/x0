if(ENABLE_EXAMPLES)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src/)
	include_directories(${ASIO_INCLUDEDIR})
	include_directories(${BOOST_INCLUDEDIR})

	add_definitions(
		-Dlinux=1
		-Wall -Wno-deprecated
		-pthread
		${LLVM_CFLAGS}
		-DPACKAGE_NAME="${PACKAGE_NAME}"
		-DPACKAGE_VERSION="${PACKAGE_VERSION}"
		-DPACKAGE_HOMEPAGE_URL="${PACKAGE_HOMEPAGE_URL}"
		-DSYSCONFDIR="${SYSCONFDIR}"
		-DVERSION="${PACKAGE_VERSION}")

	set(CMAKE_C_FLAGS "-std=c11")
	set(CMAKE_CXX_FLAGS "-std=c++11")

	add_executable(app1 app1.cpp)
	target_link_libraries(app1 XzeroBase XzeroHttp)

	add_executable(capi-app1 capi-app1.c)
	target_link_libraries(capi-app1 XzeroBase x0capi)

	add_executable(capi-post-data capi-post-data.c)
	target_link_libraries(capi-post-data XzeroBase x0capi)

	add_executable(tcp-echo-server tcp-echo-server.cpp)
	target_link_libraries(tcp-echo-server XzeroBase)
endif()
