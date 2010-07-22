/* <x0/plugins/ssl/SslSocket.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "SslSocket.h"
#include "SslDriver.h"
#include "SslContext.h"
#include <x0/Defines.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>

#include <gnutls/gnutls.h>
#include <gnutls/extra.h>

#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("SslSocket: " msg)
#endif

SslSocket::SslSocket(SslDriver *driver, int fd) :
	x0::Socket(driver->loop_, fd),
	ctime_(ev_now(driver->loop_)),
	driver_(driver),
	context_(NULL),
	session_()
{
	//TRACE("SslSocket()");

	static int protocolPriorities_[] = { GNUTLS_TLS1_2, GNUTLS_TLS1_1, GNUTLS_TLS1_0, GNUTLS_SSL3, 0 };

	setSecure(true);
	setState(HANDSHAKE);

	gnutls_init(&session_, GNUTLS_SERVER);
	gnutls_protocol_set_priority(session_, protocolPriorities_);

	gnutls_handshake_set_post_client_hello_function(session_, &SslSocket::onClientHello);

	gnutls_certificate_server_set_request(session_, GNUTLS_CERT_REQUEST);
	gnutls_dh_set_prime_bits(session_, 1024);

	gnutls_session_enable_compatibility_mode(session_);

	gnutls_session_set_ptr(session_, this);
	gnutls_transport_set_ptr(session_, (gnutls_transport_ptr_t)(handle()));

	driver_->cache(this);
}

SslSocket::~SslSocket()
{
	//TRACE("~SslSocket()");
	gnutls_deinit(session_);
}

int SslSocket::onClientHello(gnutls_session_t session)
{
	//TRACE("onClientHello()");

	SslSocket *socket = (SslSocket *)gnutls_session_get_ptr(session);

	// find SNI server
	const int MAX_HOST_LEN = 255;
	std::size_t dataLen = MAX_HOST_LEN;
	char sniName[MAX_HOST_LEN];
	unsigned int sniType;

	int rv = gnutls_server_name_get(session, sniName, &dataLen, &sniType, 0);
	if (rv != 0)
	{
		TRACE("onClientHello(): gnutls_server_name_get() failed with (%d): %s", rv, gnutls_strerror(rv));
		return GNUTLS_E_UNIMPLEMENTED_FEATURE;
	}

	if (sniType != GNUTLS_NAME_DNS)
	{
		TRACE("onClientHello(): Unknown SNI type: %d", sniType);
		return GNUTLS_E_UNIMPLEMENTED_FEATURE;
	}

	//TRACE("onClientHello(): SNI Name: \"%s\"", sniName);

	if (SslContext *cx = socket->driver_->selectContext(sniName))
		cx->bind(socket);

	return 0;
}

void SslSocket::handshake()
{
	//TRACE("handshake()");
	int rv = gnutls_handshake(session_);

	if (rv == GNUTLS_E_SUCCESS)
	{
		// handshake either completed or failed
		//TRACE("SSL handshake complete. (time: %.4f)", ev_now(loop()) - ctime_);

		setState(OPERATIONAL);
		setMode(READ);
		callback();
	}
	else if (rv != GNUTLS_E_AGAIN && rv != GNUTLS_E_INTERRUPTED)
	{
		TRACE("SSL handshake failed (%d): %s", rv, gnutls_strerror(rv));

		setState(FAILURE);
		callback();
	}
	else
	{
		//TRACE("SSL partial handshake: (%d)", gnutls_record_get_direction(session_));
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
	}
}

ssize_t SslSocket::read(x0::Buffer& result)
{
	if (result.size() == result.capacity())
		result.reserve(result.size() + 4096);

	ssize_t rv = gnutls_read(session_, result.end(), result.capacity() - result.size());
	if (rv < 0)
		return rv;

	result.resize(result.size() + rv);
	return rv;
}

ssize_t SslSocket::write(const x0::BufferRef& source)
{
	return gnutls_write(session_, source.data(), source.size());
}

ssize_t SslSocket::write(int fd, off_t *offset, size_t nbytes)
{
	char buf[4096];
	ssize_t rv = pread(fd, buf, std::min(sizeof(buf), nbytes), *offset);

	if (rv > 0)
	{
		rv = gnutls_write(session_, buf, rv);

		if (rv > 0)
			*offset += rv;
	}

	return rv;
}
