#ifndef sw_x0_ssl_db_cache_hpp
#define sw_x0_ssl_db_cache_hpp 1

#include <x0/sysconfig.h>

#if defined(WITH_SSL)

#include <cstring>
#include <gnutls/gnutls.h>

namespace x0 {

/** implements SSL cache, used for session resuming by GnuTLS library.
 */
class ssl_db_cache
{
	ssl_db_cache(const ssl_db_cache&) = delete;
	ssl_db_cache& operator=(const ssl_db_cache&) = delete;

private:
	struct item
	{
		char key_[64];
		std::size_t key_size_;
		char value_[1024];
		std::size_t value_size_;

		item()
		{
			clear();
		}

		void clear()
		{
			std::memset(key_, 0, sizeof(key_));
			std::memset(value_, 0, sizeof(value_));

			reset();
		}

		void reset()
		{
			key_size_ = 0;
			value_size_ = 0;
		}

		bool equals(const gnutls_datum_t& key) const
		{
			return key.size == key_size_
				&& std::memcmp(key.data, key_, key.size) == 0;
		}

		void set(const gnutls_datum_t& key, const gnutls_datum_t& value)
		{
			std::memcpy(key_, key.data, key.size);
			key_size_ = key.size;

			std::memcpy(value_, value.data, value.size);
			value_size_ = value.size;
		}
	};

	item *items_;
	std::size_t size_;
	std::size_t ptr_;

public:
	explicit ssl_db_cache(std::size_t size) :
		items_(new item[size]),
		size_(size),
		ptr_(0)
	{
	}

	~ssl_db_cache()
	{
		delete[] items_;
	}

	bool store(const gnutls_datum_t& key, const gnutls_datum_t& value)
	{
		if (size_ == 0)
			return false;

		if (key.size > 64)
			return false;

		if (value.size > 1024)
			return false;

		items_[ptr_].set(key, value);

		ptr_++;
		ptr_ %= size_;

		return true;
	}

	gnutls_datum_t retrieve(const gnutls_datum_t& key) const
	{
		gnutls_datum_t result = { NULL, 0 };

		for (const item *i = items_, *e = i + size_; i != e; ++i)
		{
			if (i->equals(key))
			{
				result.data = (unsigned char *)gnutls_malloc(i->value_size_);

				if (result.data)
				{
					std::memcpy(result.data, i->value_, i->value_size_);
					result.size = i->value_size_;
				}
				break;
			}
		}

		return result;
	}

	bool remove(gnutls_datum_t key)
	{
		for (item *i = items_, *e = i + size_; i != e; ++i)
		{
			if (i->equals(key))
			{
				i->reset();
				return true;
			}
		}

		return false;
	}

	void bind(gnutls_session_t session)
	{
		gnutls_db_set_ptr(session, this);
		gnutls_db_set_store_function(session, &ssl_db_cache::_store);
		gnutls_db_set_remove_function(session, &ssl_db_cache::_remove);
		gnutls_db_set_retrieve_function(session, &ssl_db_cache::_retrieve);
	}

private:
	static int _store(void *dbf, gnutls_datum_t key, gnutls_datum_t value);
	static gnutls_datum_t _retrieve(void *dbf, gnutls_datum_t key);
	static int _remove(void *dbf, gnutls_datum_t key);
};

} // namespace x0

#endif // WITH_SSL

#endif
