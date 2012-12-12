# - Find libev
# Find the native MYSQL includes and library
#
#  MYSQL_INCLUDE_DIR    - where to find includes, etc.
#  MYSQL_LIBRARY_DIR    - List of libraries when using libev.

find_program(MYSQL_CONFIG_EXECUTABLE NAMES mysql_config DOC "path to mysql_config executable")

execute_process(
	COMMAND sh -c "${MYSQL_CONFIG_EXECUTABLE} --cflags | sed -e 's/-DNDEBUG\\(=1\\)\\?//g'"
	OUTPUT_VARIABLE MYSQL_CFLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${MYSQL_CONFIG_EXECUTABLE} --libdir
	OUTPUT_VARIABLE MYSQL_LIBRARY_DIR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${MYSQL_CONFIG_EXECUTABLE} --libs
	OUTPUT_VARIABLE MYSQL_LDFLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${MYSQL_CONFIG_EXECUTABLE} --libs_r
	OUTPUT_VARIABLE MYSQL_LDFLAGS_R
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${MYSQL_CONFIG_EXECUTABLE} --version
	OUTPUT_VARIABLE MYSQL_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "mySQL config    : " ${MYSQL_CONFIG_EXECUTABLE})
message(STATUS "mySQL version   : " ${MYSQL_VERSION})
message(STATUS "mySQL cflags    : " ${MYSQL_CFLAGS})
message(STATUS "mySQL ldflags   : " ${MYSQL_LDFLAGS})
message(STATUS "mySQL ldflags_r : " ${MYSQL_LDFLAGS_R})
