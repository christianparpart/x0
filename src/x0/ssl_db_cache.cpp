#include <x0/ssl_db_cache.h>

#if defined(WITH_SSL)

namespace x0 {

int ssl_db_cache::_store(void *dbf, gnutls_datum_t key, gnutls_datum_t value)
{
	if (reinterpret_cast<ssl_db_cache *>(dbf)->store(key, value))
		return 0;

	return -1;
}

gnutls_datum_t ssl_db_cache::_retrieve(void *dbf, gnutls_datum_t key)
{
	return reinterpret_cast<ssl_db_cache *>(dbf)->retrieve(key);
}

int ssl_db_cache::_remove(void *dbf, gnutls_datum_t key)
{
	if (reinterpret_cast<ssl_db_cache *>(dbf)->remove(key))
		return 0;

	return -1;
}

} // namespace x0

#endif
