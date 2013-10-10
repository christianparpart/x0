include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory(src)
add_subdirectory(plugins)

if(ENABLE_X0D)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/x0d/include
	DESTINATION ${INCLUDEDIR}
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "*.cc")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/x0d.pc.cmake ${CMAKE_CURRENT_BINARY_DIR}/x0d.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/x0d.pc DESTINATION ${LIBDIR}/pkgconfig)

endif()
