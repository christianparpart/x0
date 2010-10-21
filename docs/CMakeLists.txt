add_custom_command(
	OUTPUT x0d.8 
	DEPENDS x0d.8.pod
	COMMAND pod2man --section 8 --release 
		--center "x0d - high performance HTTP web server"
		${CMAKE_CURRENT_SOURCE_DIR}/x0d.8.pod >${CMAKE_CURRENT_BINARY_DIR}/x0d.8
)

add_custom_target(man ALL DEPENDS x0d.8)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/x0d.8
	DESTINATION ${CMAKE_INSTALL_PREFIX}/man/man8)
