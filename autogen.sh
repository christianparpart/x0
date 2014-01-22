#! /bin/bash

if [[ -z "$CXX" ]]; then
	if which clang++ &>/dev/null; then
		export CXX=$(which clang++)
	else
		export CXX=$(which g++)
	fi
	if which clang &>/dev/null; then
		export CC=$(which clang)
	else
		export CC=$(which cc)
	fi
fi

if [[ "$1" == "clean" ]]; then
	rm -vf {configure,build,install}-stamp
	rm -vf debian/*.log
	rm -vf debian/*.substvars
	rm -vf debian/*.debhelper
	rm -rvf debian/{files,tmp,x0d,x0d-plugins}
	rm -rvf debian/libxzero-{base,http,flow}{,-dev}

	rm -vf x0d/src/x0d
	find . \( -name 'CMakeCache.txt' -o -name 'CMakeFiles' \
			-o -name 'Makefile' -o -name cmake_install.cmake \
			-o -name '*.so' -o -name '*.a' \
			-o -name 'vgcore.*' -o -name core \
			\) \
		-exec rm -rf {} \; 2>/dev/null
else
	cmake "$(dirname $0)" \
		-DCMAKE_CXX_COMPILER="${CXX}" \
		-DCMAKE_C_COMPILER="${CC}" \
		-DCMAKE_CXX_FLAGS_DEBUG="-O0 -g3" \
		-DCMAKE_BUILD_TYPE="debug" \
		-DCMAKE_INSTALL_PREFIX="${HOME}/local" \
		-DENABLE_PLUGIN_{RRD,IMAGEABLE,WEBDAV}=ON \
		-DENABLE_EXAMPLES=ON \
		-DENABLE_TESTS=ON
fi
