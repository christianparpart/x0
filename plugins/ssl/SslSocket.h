/* <SslSocket.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_SslSocket_h
#define sw_x0_SslSocket_h (1)

#include <x0/Socket.h>
#include <x0/sysconfig.h>

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#endif

class SslDriver;
class SslContext;

/** \brief SSL socket.
 */
class SslSocket :
	public x0::Socket
{
private:
#ifndef NDEBUG
	ev_tstamp ctime_;
#endif

	SslDriver *driver_;
	const SslContext *context_;
	gnutls_session_t session_;		//!< SSL (GnuTLS) session handle

	friend class SslDriver;
	friend class SslContext;

	static int onClientHello(gnutls_session_t session);

public:
	explicit SslSocket(SslDriver *driver, struct ev_loop *loop, int fd);
	virtual ~SslSocket();

	const SslContext *context() const;

public:
	// synchronous non-blocking I/O
	virtual ssize_t read(x0::Buffer& result);
	virtual ssize_t write(const x0::BufferRef& source);
	virtual ssize_t write(int fd, off_t *offset, size_t nbytes);

protected:
	virtual void handshake();
};

// {{{ inlines
inline const SslContext *SslSocket::context() const
{
	return context_;
}
// }}}

#endif
