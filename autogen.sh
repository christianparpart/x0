#! /bin/bash

#if [ -z "$CXX" ]; then
#	if which clang++ &>/dev/null; then
#		export CXX=$(which clang++)
#	fi
#fi

if [ "$1" == "clean" ]; then
	find . -name 'CMakeCache.txt' -print | xargs rm -vrf &>/dev/null
	find . -name 'CMakeFiles*' -print | xargs rm -vrf &>/dev/null
	find . -name 'Makefile' -exec rm -f {} \;
	rm -f cmake_install.cmake
else
	cmake "$(dirname $0)" \
		-DCMAKE_CXX_FLAGS_DEBUG="-O0 -g3" \
		-DCMAKE_BUILD_TYPE="debug" \
		-DENABLE_RRD=ON \
		-DENABLE_WEBDAV=ON \
		-DENABLE_EXAMPLES=ON \
		-DCMAKE_INSTALL_PREFIX="/opt/sandbox"
fi
