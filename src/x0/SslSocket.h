#ifndef sw_x0_SslSocket_h
#define sw_x0_SslSocket_h (1)

#include <x0/Socket.h>
#include <x0/sysconfig.h>

#if defined(WITH_SSL)
#	include <gnutls/gnutls.h>
#endif

namespace x0 {

class SslDriver;

/** \brief SSL socket.
 */
class SslSocket :
	public Socket
{
private:
	SslDriver *driver_;
	gnutls_session_t session_;		//!< SSL (GnuTLS) session handle
	bool handshaking_;

	friend class SslDriver;

public:
	explicit SslSocket(SslDriver *driver, int fd);
	virtual ~SslSocket();

	bool initialize();
	bool isSecure() const;

public:
	// synchronous non-blocking I/O
	virtual ssize_t read(Buffer& result);
	virtual ssize_t write(const BufferRef& source);
	virtual ssize_t write(int fd, off_t *offset, size_t nbytes);

private:
	bool handshake();
};

} // namespace x0

#endif
