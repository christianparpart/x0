#! /bin/bash

if [ "$1" == "clean" ]; then
	find . -name 'CMakeCache.txt' -print | xargs rm -vrf &>/dev/null
	find . -name 'CMakeFiles*' -print | xargs rm -vrf &>/dev/null
else
	cmake . \
		-DASIO_INCLUDEDIR="$(dirname $(pwd))/asio/include" \
		-DCMAKE_CXX_FLAGS_DEBUG="-O0 -ggdb3" \
		-DCMAKE_BUILD_TYPE="debug"
fi
