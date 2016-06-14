#! /bin/bash

ROOT=`dirname $0`

FILES=(
  /ar-lib
  /aclocal.m4
  /compile
  /autom4te.cache
  /configure
  /install-sh
  /missing
  /depcomp
  /src/xzero/sysconfig.h.in
  /src/xzero-flow/sysconfig.h.in
)

if test "$1" == "clean"; then
  find $ROOT -name Makefile.in -exec rm {} \;
  for file in ${FILES[*]}; do rm -vrf "${ROOT}${file}"; done
  exit 0
fi

exec autoreconf --verbose --force --install $ROOT
