#ifndef sw_x0_SslDriver_h
#define sw_x0_SslDriver_h (1)

#include <x0/SocketDriver.h>
#include <x0/SslSocket.h>
#include <gnutls/gnutls.h>

namespace x0 {

struct SslCacheItem;

class SslDriver : public SocketDriver
{
private:
	struct ev_loop *loop_;

	std::string crlFileName_;
	std::string trustFileName_;
	std::string keyFileName_;
	std::string certFileName_;

	gnutls_certificate_credentials_t x509Cred_;
	gnutls_dh_params_t dhParams_;
	gnutls_priority_t priorityCache_;

	SslCacheItem *items_;
	std::size_t size_;
	std::size_t ptr_;

	friend class SslSocket;

public:
	explicit SslDriver(struct ev_loop *loop);
	virtual ~SslDriver();

	virtual SslSocket *create(int handle);
	virtual void destroy(Socket *);

	void cache(SslSocket *socket);

private:
	bool store(const gnutls_datum_t& key, const gnutls_datum_t& value);
	gnutls_datum_t retrieve(const gnutls_datum_t& key) const;
	bool remove(gnutls_datum_t key);

	static int _store(void *dbf, gnutls_datum_t key, gnutls_datum_t value);
	static gnutls_datum_t _retrieve(void *dbf, gnutls_datum_t key);
	static int _remove(void *dbf, gnutls_datum_t key);
};

} // namespace x0

#endif
