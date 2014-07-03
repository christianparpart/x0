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
    rm -rf docs/html
    rm -vf XzeroBase.pc XzeroFlow.pc XzeroHttp.pc x0d/x0d.pc
    rm -vf install_manifest.txt

	rm -vf "x0d/src/x0d" \
           "examples/app1" \
           "flow-tool/flow-tool" \
           "docs/flow.7" \
           "docs/x0d.8" \
           "docs/x0d.conf.5" \
           "include/x0/sysconfig.h" \
           "examples/capi-app1" \
           "examples/capi-app2" \
           "examples/capi-post-data" \
           "examples/tcp-echo-server" \
           "examples/tcp-echo-server-splice" \
           "tests/fcgi-long-run" \
           "tests/fcgi-staticfile" \
           "tests/queue-bench" \
           "tests/splice_test" \
           "tests/test_flow_lexer" \
           "tests/x0cp" \
           "tests/x0test" \
           "tests/x0test" \
           "tests/x0-test"

	find . \( -name 'CMakeCache.txt' -o -name 'CMakeFiles' \
			-o -name 'Makefile' -o -name cmake_install.cmake \
			-o -name '*.so' \
            -o -name '*.a' \
			-o -name '*.so.*' \
			-o -name 'vgcore.*' -o -name core \
            -o -name '.directory' \
			\) \
		-exec rm -rf {} \; 2>/dev/null
else
	cmake "$(dirname $0)" \
		-DCMAKE_BUILD_TYPE="debug" \
		-DCMAKE_CXX_FLAGS_DEBUG="-O0 -g3" \
		-DCMAKE_INSTALL_PREFIX="${HOME}/local" \
		-DENABLE_PLUGIN_{RRD,IMAGEABLE,WEBDAV,DEBUG}=ON \
		-DENABLE_FLOW_TOOL=ON \
		-DENABLE_EXAMPLES=ON \
		-DENABLE_TESTS=ON
fi
