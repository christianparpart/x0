#! /bin/bash

if [ "$1" == "clean" ]; then
	find . -name 'CMakeCache.txt' -print | xargs rm -vrf
	find . -name 'CMakeFiles*' -print | xargs rm -vrf
else
	cmake .
fi
