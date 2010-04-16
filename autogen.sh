#! /bin/bash

if [ "$1" == "clean" ]; then
	find . -name 'CMakeCache.txt' -print | xargs rm -vrf &>/dev/null
	find . -name 'CMakeFiles*' -print | xargs rm -vrf &>/dev/null
	rm -f cmake_install.cmake
else
	cmake . \
		-DASIO_INCLUDEDIR="$(dirname $(pwd))/asio/include" \
		-DCMAKE_CXX_FLAGS_DEBUG="-O0 -ggdb3" \
		-DCMAKE_INSTALL_PREFIX="/opt/sandbox" \
		-DSYSCONFDIR="/opt/sandbox/etc" \
		-DINCLUDEDIR="/opt/sandbox/include" \
		-DLIBDIR="/opt/sandbox/lib64" \
		-DCMAKE_BUILD_TYPE="debug"
fi
