# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

inherit eutils

DESCRIPTION="x0 HTTP web server"
HOMEPAGE="http://xzero.io/"
SRC_URI="http://files.xero.ws/${PN}/${P}.tar.gz"
LICENSE="AGPL-v3"

SLOT="0"
KEYWORDS="~x86 ~amd64"
IUSE="ssl"

#RESTRICT="strip"

DEPEND=""

RDEPEND="${DEPEND}"

src_compile() {
	cmake . \
		-DCMAKE_BUILD_TYPE="release" \
		-DCMAKE_INSTALL_PREFIX="/usr" \
		-DSYSCONFDIR="/etc/x0" \
		-DINCLUDEDIR="/usr/include" \
		-DLIBDIR="/usr/lib" \
		-DLOCALSTATEDIR="/var/run/x0" \
		|| die "cmake failed"

	emake || die "emake failed"
}

src_install() {
	emake DESTDIR="${D}" install || die "emake install failed"
}
