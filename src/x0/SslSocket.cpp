#include <x0/SslSocket.h>
#include <x0/SslDriver.h>
#include <x0/Defines.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>

#include <gnutls/gnutls.h>
#include <gnutls/extra.h>

#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("HttpConnection: " msg)
#endif

namespace x0 {

SslSocket::SslSocket(SslDriver *driver, int fd) :
	Socket(driver->loop_, fd),
	driver_(driver),
	session_(),
	handshaking_(true)
{
}

SslSocket::~SslSocket()
{
	gnutls_deinit(session_);
}

bool SslSocket::initialize()
{
	gnutls_init(&session_, GNUTLS_SERVER);
	gnutls_priority_set(session_, driver_->priorityCache_);
	gnutls_credentials_set(session_, GNUTLS_CRD_CERTIFICATE, driver_->x509Cred_);

	gnutls_certificate_server_set_request(session_, GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits(session_, 1024);

	gnutls_session_enable_compatibility_mode(session_);

	gnutls_transport_set_ptr(session_, (gnutls_transport_ptr_t)(handle()));

	driver_->cache(this);

	return handshake();
}

bool SslSocket::handshake()
{
	int rv = gnutls_handshake(session_);
	if (rv == GNUTLS_E_SUCCESS)
	{
		// handshake either completed or failed
		handshaking_ = false;
		//TRACE("SSL handshake time: %.4f", ev_now(loop_) - ctime_);
		setMode(READ);
		return true;
	}

	if (rv != GNUTLS_E_AGAIN && rv != GNUTLS_E_INTERRUPTED)
	{
		TRACE("SSL handshake failed (%d): %s", rv, gnutls_strerror(rv));

		//delete this; // FIXME old code from HttpConnection not copy'n'pastable
		return false;
	}

	TRACE("SSL partial handshake: (%d)", gnutls_record_get_direction(session_));
	switch (gnutls_record_get_direction(session_))
	{
		case 0: // read
			setMode(READ);
			break;
		case 1: // write
			setMode(WRITE);
			break;
		default:
			break;
	}
	return false;
}

bool SslSocket::isSecure() const
{
	return true;
}

ssize_t SslSocket::read(Buffer& result)
{
	if (result.size() == result.capacity())
		result.reserve(result.size() + 4096);

	ssize_t rv = gnutls_read(session_, result.end(), result.capacity() - result.size());
	if (rv < 0)
		return rv;

	result.resize(result.size() + rv);
	return rv;
}

ssize_t SslSocket::write(const BufferRef& source)
{
	ssize_t rv = gnutls_write(session_, source.data(), source.size());
	return rv;
}

ssize_t SslSocket::write(int fd, off_t *offset, size_t nbytes)
{
	char buf[4096];
	ssize_t rv = pread(fd, buf, std::min(sizeof(buf), nbytes), *offset);

	if (rv > 0)
	{
		rv = gnutls_write(session_, buf, rv);

		if (rv >= 0)
			*offset += rv;
	}

	return 0;
}

} // namespace x0
