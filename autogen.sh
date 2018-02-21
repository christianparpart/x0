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

findexe() {
  for exe in ${@}; do
    if which $exe 2>/dev/null; then
      return
    fi
  done
  echo $1
}

# Mac OSX has a special location for more recent LLVM/clang installations
#   $ brew tap homebrew/versions
#   $ brew install llvm
if [[ -d "/usr/local/opt/llvm/bin" ]]; then
  export PATH="/usr/local/opt/llvm/bin:${PATH}"
  export CXXFLAGS="$CXXFLAGS -nostdinc++ -I/usr/local/opt/llvm/include/c++/v1"
  export LDFLAGS="$LDFLAGS -L/usr/local/opt/llvm/lib"
fi

# Mac OS/X has `brew install zlib`'d its zlib.pc somewhere non-standard ;-)
pkgdirs=( "/usr/local/opt/zlib/lib/pkgconfig" )
for pkgdir in ${pkgdirs[*]}; do
  if [[ -d "${pkgdir}" ]]; then
    export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}${PKG_CONFIG_PATH:+:}${pkgdir}
  fi
done

export CXX=$(findexe $CXX clang++-5.0 clang++-4.0 clang++ g++)
export CC=$(findexe $CC clang-5.0 clang-4.0 clang gcc)
export CXXFLAGS="-O0 -g"

echo CXX = $CXX
echo CC = $CC
echo CXXFLAGS = $CXXFLAGS
echo PKG_CONFIG_PATH = $PKG_CONFIG_PATH

if [[ "${1}" == "cmake" ]]; then
  shift
  exec cmake "${ROOT}" \
              -DCMAKE_BUILD_TYPE="debug" \
              -DCMAKE_INSTALL_PREFIX="${HOME}/local" \
              -DCMAKE_VERBOSE_MAKEFILE=OFF \
              "${@}"
fi

# -----------------------------------------------------------------------------

[[ "${1}" != "--help" ]] && autoreconf --verbose --force --install ${ROOT}

exec ${ROOT}/configure --prefix="${HOME}/usr" \
                       --sysconfdir="${HOME}/usr/etc" \
                       --with-pidfile="${BUILDDIR}/x0d.pid" \
                       --with-logdir="${BUILDDIR}" \
                       --enable-proxy \
                       --enable-xurl \
                       "${@}"
