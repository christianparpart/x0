# libev

# libev does not provide any installation configuration script,
# so we have to make the standard-guess and let the user override 
# to the actual values if they differ.

set(EV_INCLUDE_DIR /usr/include CACHE PATH "libev include dir")
set(EV_LIBRARY_DIR /usr/lib CACHE PATH "libev library dir")
set(EV_LIBRARIES ev CACHE STRING "libev library (ldflags)")
set(EV_CPPFLAGS CACHE STRING "libev C preprocessor flags")
