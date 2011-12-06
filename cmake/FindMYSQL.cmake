# - Find libev
# Find the native MYSQL includes and library
#
#  MYSQL_INCLUDE_DIR    - where to find includes, etc.
#  MYSQL_LIBRARY_DIR    - List of libraries when using libev.

find_program(MYSQL_CONFIG_EXECUTABLE NAMES mysql_config DOC "path to mysql_config executable")

# TODO: strip off potential -NDEBUG=1 flags
execute_process(
	COMMAND ${MYSQL_CONFIG_EXECUTABLE} --cflags OUTPUT_VARIABLE MYSQL_CFLAGS
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

message("mySQL config    : " ${MYSQL_CONFIG_EXECUTABLE})
message("mySQL version   : " ${MYSQL_VERSION})
message("mySQL cflags    : " ${MYSQL_CFLAGS})
message("mySQL ldflags   : " ${MYSQL_LDFLAGS})
message("mySQL ldflags_r : " ${MYSQL_LDFLAGS_R})
