# libbase.pc.cmake

Name: libbase
Description: C++ base framework, containing general-purpose APIs
Version: @LIBBASE_VERSION@
Requires: libpcre >= 7.0 
# Conflicts: 
Libs: -L@CMAKE_INSTALL_PREFIX@/lib -lbase -lev -lpthread -pthread @POSIX_RT_LIBS@ @MYSQL_LDFLAGS@
Cflags: -I@CMAKE_INSTALL_PREFIX@/include -pthread @CFLAGS_RDYNAMIC@ @MYSQL_CFLAGS@ @CXXFLAGS@
