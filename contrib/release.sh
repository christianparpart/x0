#! /bin/bash
# vim:ts=2:sw=2

# switch to project root
cd $(dirname $0)/..

VERSION=`head -n1 debian/changelog | sed 's/^.*(\(.*\)).*$/\1/'`

# build source-only
dpkg-buildpackage -S || exit $?

# upload to PPA
dput -f ppa:trapni/xzero ../x0_${VERSION}_source.changes || exit $?
