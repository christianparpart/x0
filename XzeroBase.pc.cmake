# XzeroBase.pc.cmake

Name: XzeroBase
Description: x0 framework (XzeroBase, base module)
Version: @PACKAGE_VERSION@
Requires: libpcre >= 7.0 
# Conflicts: 
Libs: -L@LIBDIR@ @LIBS_RPATH@ -lXzeroBase -lpthread @POSIX_RT_LIBS@ @MYSQL_LDFLAGS@
Cflags: -I@INCLUDEDIR@ -pthread @CFLAGS_RDYNAMIC@ @MYSQL_CFLAGS@ @CXXFLAGS@
