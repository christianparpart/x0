find_path(FCGI_INCLUDE_PATH fcgiapp.h PATHS ${FCGI_INCLUDE_DIR})
if(FCGI_INCLUDE_PATH)
	set(FCGI_HEADER fcgi_stdio.h CACHE INTERNAL "")
	set(HAVE_FCGI_STDIO_H 1 CACHE INTERNAL "")
endif()

if(NOT FCGI_HEADER)
	message(FATAL_ERROR "fcgi includes not found.")
endif()

find_library(FCGI_LIBRARY_PATH fcgi PATHS ${FCGI_LIBRARY_DIR})
if(NOT FCGI_LIBRARY_PATH)
	message(FATAL_ERROR "libfcgi not found.")
endif()

message(STATUS "FCGI include path: ${FCGI_INCLUDE_PATH}")
message(STATUS "FCGI library path: ${FCGI_LIBRARY_PATH}")
