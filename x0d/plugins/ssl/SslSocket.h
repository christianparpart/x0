/* <SslSocket.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_SslSocket_h
#define sw_x0_SslSocket_h (1)

#include <x0/Socket.h>
#include <x0/sysconfig.h>

#include <gnutls/gnutls.h>

class SslDriver;
class SslContext;

/** \brief SSL socket.
 */
class SslSocket :
	public x0::Socket
{
private:
#ifndef XZERO_NDEBUG
	ev_tstamp ctime_;
#endif

	SslDriver *driver_;
	const SslContext *context_;
	gnutls_session_t session_;		//!< SSL (GnuTLS) session handle

	friend class SslDriver;
	friend class SslContext;

	static int onClientHello(gnutls_session_t session);

public:
	explicit SslSocket(SslDriver *driver, struct ev_loop *loop, int fd, int af);
	virtual ~SslSocket();

	const SslContext *context() const;

public:
	// synchronous non-blocking I/O
	virtual ssize_t read(x0::Buffer& result);
	virtual ssize_t write(const void *buffer, size_t size);
	virtual ssize_t write(int fd, off_t *offset, size_t nbytes);

protected:
	virtual void handshake(int revents);
};

// {{{ inlines
inline const SslContext *SslSocket::context() const
{
	return context_;
}
// }}}

#endif
