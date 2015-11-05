# xzero-flow.pc.cmake

Name: xzero-flow
Description: Flow Configuration Language Framework
Version: @XZERO_FLOW_VERSION@
Requires: xzero >= 0.10.0
# Conflicts: 
Libs: -L@CMAKE_INSTALL_PREFIX@/lib -lxzero-flow
Cflags: -I@CMAKE_INSTALL_PREFIX@/include
