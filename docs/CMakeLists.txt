add_custom_command(
	OUTPUT x0d.8 
	DEPENDS x0d.8.pod
	COMMAND pod2man --name="x0d" --section 8 --release --utf8 --stderr
		--center "x0d - high performance HTTP web server"
		${CMAKE_CURRENT_SOURCE_DIR}/x0d.8.pod >${CMAKE_CURRENT_BINARY_DIR}/x0d.8
)

add_custom_command(
	OUTPUT x0d.conf.5 
	DEPENDS x0d.conf.5.pod
	COMMAND pod2man --name="x0d.conf" --section 5 --release --utf8 --stderr
		--center "x0d.conf - high performance HTTP web server configuration"
		${CMAKE_CURRENT_SOURCE_DIR}/x0d.conf.5.pod >${CMAKE_CURRENT_BINARY_DIR}/x0d.conf.5
)

add_custom_target(man ALL DEPENDS x0d.8 x0d.conf.5)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/x0d.8
	DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man8)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/x0d.conf.5
	DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man5)
