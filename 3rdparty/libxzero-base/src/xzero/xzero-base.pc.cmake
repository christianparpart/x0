# libcortex-base.pc.cmake

Name: libcortex-base
Description: C++ Cortex Framework, containing general-purpose APIs
Version: @XZERO_BASE_BASE_VERSION@
Requires: libpcre >= 7.0 
# Conflicts: 
Libs: -L@CMAKE_INSTALL_PREFIX@/lib -lcortex_base -lpthread -pthread @POSIX_RT_LIBS@
Cflags: -I@CMAKE_INSTALL_PREFIX@/include -pthread @CFLAGS_RDYNAMIC@ @CXXFLAGS@
