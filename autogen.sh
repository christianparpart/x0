#! /bin/bash

set -e

BUILDDIR=`pwd`
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
  /xzero/sysconfig.h.in
  /xzero-flow/sysconfig.h.in
)

if test "$1" == "clean"; then
  find ${ROOT} -name Makefile.in -exec rm {} \;
  for file in ${FILES[*]}; do rm -vrf "${ROOT}${file}"; done
  exit 0
fi

[[ $# -ne 1 ]] && autoreconf --verbose --force --install ${ROOT}

findexe() {
  for exe in ${@}; do
    if which $exe &>/dev/null; then
      echo $exe
      return
    fi
  done
  echo $1
}

export CXX=$(findexe $CXX clang++-4.0 clang++ g++)
export CC=$(findexe $CC clang-4.0 clang gcc)
export CXXFLAGS="-O0 -g"

echo CXX = $CXX
echo CC = $CC
echo CXXFLAGS = $CXXFLAGS

exec ${ROOT}/configure --prefix="${HOME}/usr" \
                       --sysconfdir="${HOME}/usr/etc" \
                       --with-pidfile="${BUILDDIR}/x0d.pid" \
                       --with-logdir="${BUILDDIR}" \
                       --enable-xurl \
                       "${@}"
