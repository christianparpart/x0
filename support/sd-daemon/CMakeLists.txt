set(SD_SOURCES sd-daemon.c)
set(SD_HEADERS sd-daemon.h)

# GCC 4.6 complained at libx0 linking to expect libsd-daemon compiled with -fPIC flag.
add_definitions(-fPIC)

#add_library(sd-daemon STATIC ${SD_SOURCES})
add_library(sd-daemon OBJECT ${SD_SOURCES})
set_target_properties(sd-daemon PROPERTIES ENABLE_EXPORTS ON)

#if(NOT APPLE)
#target_link_libraries(sd-daemon rt)
#endif()
