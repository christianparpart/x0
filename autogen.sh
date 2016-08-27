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

autoreconf --verbose --force --install $ROOT

findexe() {
  for exe in ${@}; do
    if which $exe &>/dev/null; then
      echo $exe
      return
    fi
  done
  echo $1
}

export CXX=$(findexe clang++-3.5 clang++)
export CC=$(findexe clang-3.5 clang)
export CXXFLAGS="-O0 -ggdb3"

echo CXX = $CXX
echo CC = $CC
echo CXXFLAGS = $CXXFLAGS

$ROOT/configure --prefix="/usr" \
                --sysconfdir="$HOME/projects/x0" \
                --runstatedir="$HOME/projects/x0/build" \
                --with-pidfile="$HOME/projects/x0/build/x0d.pid" \
                --with-logdir="$HOME/projects/x0/x0d"

