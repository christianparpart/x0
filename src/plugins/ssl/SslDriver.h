/* <SslDriver.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_SslDriver_h
#define sw_x0_SslDriver_h (1)

#include <x0/SocketDriver.h>
#include "SslContext.h" // SslContextSelector
#include "SslSocket.h"
#include <gnutls/gnutls.h>

struct SslCacheItem;
struct SslContext;

class SslDriver :
	public x0::SocketDriver
{
private:
	struct ev_loop *loop_;
	SslContextSelector *selector_;

	SslCacheItem *items_;
	std::size_t size_;
	std::size_t ptr_;

	friend class SslSocket;
	friend class SslContext;

public:
	SslDriver(struct ev_loop *loop, SslContextSelector *selector);
	virtual ~SslDriver();

	virtual bool isSecure() const;

	virtual SslSocket *create(int handle);
	virtual void destroy(x0::Socket *);

	void cache(SslSocket *socket);

	SslContext *selectContext(const std::string& dnsName) const;

	const SslContextSelector *selector() const;

private:
	bool store(const gnutls_datum_t& key, const gnutls_datum_t& value);
	gnutls_datum_t retrieve(const gnutls_datum_t& key) const;
	bool remove(gnutls_datum_t key);

	static int _store(void *dbf, gnutls_datum_t key, gnutls_datum_t value);
	static gnutls_datum_t _retrieve(void *dbf, gnutls_datum_t key);
	static int _remove(void *dbf, gnutls_datum_t key);
};

// {{{ inlines
inline const SslContextSelector *SslDriver::selector() const
{
	return selector_;
}
// }}}

#endif
