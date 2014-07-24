# libbase.pc.cmake

Name: libbase
Description: C++ base framework, containing commonly shared APIs
Version: @PACKAGE_VERSION@
Requires: libpcre >= 7.0 
# Conflicts: 
Libs: -L@LIBDIR@ @LIBS_RPATH@ -lbase -lpthread @POSIX_RT_LIBS@ @MYSQL_LDFLAGS@
Cflags: -I@INCLUDEDIR@ -pthread @CFLAGS_RDYNAMIC@ @MYSQL_CFLAGS@ @CXXFLAGS@
