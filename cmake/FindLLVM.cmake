# - Find libev
# Find the native LLVM includes and library
#
#  LLVM_INCLUDE_DIR    - where to find includes, etc.
#  LLVM_LIBRARY_DIR    - List of libraries when using libev.

find_program(LLVM_CONFIG_EXECUTABLE NAMES llvm-config DOC "path to llvm-config executable")

# TODO: strip potential -NDEBUG=1 flags off
execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --cppflags OUTPUT_VARIABLE LLVM_CPPFLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --includedir
	OUTPUT_VARIABLE LLVM_INCLUDE_DIR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --libdir
	OUTPUT_VARIABLE LLVM_LIBRARY_DIR
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --ldflags
	OUTPUT_VARIABLE LLVM_LDFLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs engine ipo
	OUTPUT_VARIABLE LLVM_LIBRARIES
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --version
	OUTPUT_VARIABLE LLVM_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

message("LLVM config: " ${LLVM_CONFIG_EXECUTABLE})
message("LLVM cppflags: " ${LLVM_CPPFLAGS})
message("LLVM ldflags: " ${LLVM_LDFLAGS})
message("LLVM library dir: " ${LLVM_LIBRARY_DIR})
message("LLVM include dir: " ${LLVM_INCLUDE_DIR})
message("LLVM libraries: " ${LLVM_LIBRARIES})
message("LLVM version: " ${LLVM_VERSION})
