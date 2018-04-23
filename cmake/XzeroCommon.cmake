
# require a C++ compiler that at least the currently latest
# Ubuntu LTS release is supporting
if(MSVC)
# TODO ... fill in whatever is required for compiling under Visual Studio
  add_definitions(-DNOMINMAX)
  set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "/NODEFAULTLIBS:MSVCRT")
else()

# if(APPLE)
#   set(CMAKE_CXX_FLAGS "-std=c++1z -stdlib=libc++")
# else()
#   set(CMAKE_CXX_FLAGS "-std=c++1z")
# endif()

# we need the following definitions in order to get some special
# OS-level features like posix_fadvise() or readahead().
add_definitions(-DXOPEN_SOURCE=600)
add_definitions(-D_GNU_SOURCE)

# ISO C99: explicitely request format specifiers
add_definitions(-D__STDC_FORMAT_MACROS)

# enforce 64bit i/o operations, even on 32bit platforms
add_definitions(-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGE_FILES)
endif()
