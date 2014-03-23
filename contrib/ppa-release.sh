#! /bin/bash
# vim:ts=2:sw=2

# switch to project root
cd $(dirname $0)/..

VERSION=`head -n1 debian/changelog | sed 's/^.*(\(.*\)).*$/\1/'`

# always clean before building as dpkg-source will badly include just everything (FIXME: can we provide it a blacklist?)
./autogen.sh clean

# build source-only
dpkg-buildpackage -S || exit $?

# upload to PPA
dput -f ppa:trapni/xzero ../x0_${VERSION}_source.changes || exit $?
