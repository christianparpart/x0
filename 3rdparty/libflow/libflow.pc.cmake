# libflow.pc.cmake

Name: libflow
Description: Flow Configuration Language Framework
Version: @LIBFLOW_VERSION@
Requires: libbase >= 0.10.0
# Conflicts: 
Libs: -L@CMAKE_INSTALL_PREFIX@/lib -lflow
Cflags: -I@CMAKE_INSTALL_PREFIX@/include
