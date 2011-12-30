find_path(RRD_INCLUDE_PATH rrd.h PATHS ${RRD_INCLUDE_DIR})
if(RRD_INCLUDE_PATH)
	set(RRD_HEADER rrd.h CACHE INTERNAL "")
	set(HAVE_RRD_H 1 CACHE INTERNAL "")
endif()

if(NOT RRD_HEADER)
	message(FATAL_ERROR "rrd includes not found.")
endif()

find_library(RRD_LIBRARY_PATH rrd PATHS ${RRD_LIBRARY_DIR})
if(NOT RRD_LIBRARY_PATH)
	message(FATAL_ERROR "librrd not found.")
endif()

message(STATUS "RRD include path: ${RRD_INCLUDE_PATH}")
message(STATUS "RRD library path: ${RRD_LIBRARY_PATH}")
