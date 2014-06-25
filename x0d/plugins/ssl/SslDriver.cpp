// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "SslDriver.h"
#include "SslSocket.h"
#include "SslContext.h"
#include <x0/http/HttpServer.h>
#include <gnutls/gnutls.h>
#include <cstring>

#if 0
#	define TRACE(msg...) DEBUG("ssl: SslDriver: " msg)
#else
#	define TRACE(msg...)
#endif

struct SslCacheItem // {{{
{
private:
    char key_[64];
    std::size_t key_size_;
    char value_[1024];
    std::size_t value_size_;

    friend class SslDriver;

public:
    SslCacheItem();

    void clear();
    void reset();
    void set(const gnutls_datum_t& key, const gnutls_datum_t& value);
    bool equals(const gnutls_datum_t& key) const;
};

inline SslCacheItem::SslCacheItem()
{
    clear();
}

inline void SslCacheItem::clear()
{
    std::memset(key_, 0, sizeof(key_));
    std::memset(value_, 0, sizeof(value_));

    reset();
}

inline void SslCacheItem::reset()
{
    key_size_ = 0;
    value_size_ = 0;
}

inline void SslCacheItem::set(const gnutls_datum_t& key, const gnutls_datum_t& value)
{
    std::memcpy(key_, key.data, key.size);
    key_size_ = key.size;

    std::memcpy(value_, value.data, value.size);
    value_size_ = value.size;
}

inline bool SslCacheItem::equals(const gnutls_datum_t& key) const
{
    return key.size == key_size_
        && std::memcmp(key.data, key_, key.size) == 0;
}
// }}}

SslDriver::SslDriver(SslContextSelector *selector) :
    x0::SocketDriver(),
    priorities_(nullptr),
    selector_(selector),
    items_(new SslCacheItem[1024]),
    size_(1024),
    ptr_(0)
{
    TRACE("SslDriver()");
}

SslDriver::~SslDriver()
{
    TRACE("~SslDriver()");
    gnutls_priority_deinit(priorities_);
    delete[] items_;
}

bool SslDriver::isSecure() const
{
    return true;
}

void SslDriver::setPriorities(const std::string& value)
{
    TRACE("setPriorities: \"%s\"", value.c_str());

    const char *errp = nullptr;
    int rv = gnutls_priority_init(&priorities_, value.c_str(), &errp);

    if (rv != GNUTLS_E_SUCCESS) {
        TRACE("gnutls_priority_init: error: %s \"%s\"", gnutls_strerror(rv), errp ? errp : "");
    }
}

SslSocket *SslDriver::create(struct ev_loop *loop, int handle, int af)
{
    return new SslSocket(this, loop, handle, af);
}

void SslDriver::destroy(x0::Socket *socket)
{
    delete socket;
}

SslContext *SslDriver::selectContext(const std::string& dnsName) const
{
    return selector_->select(dnsName);
}

void SslDriver::initialize(SslSocket* socket)
{
    gnutls_priority_set(socket->session_, priorities_);

    // cache
    gnutls_db_set_ptr(socket->session_, this);
    gnutls_db_set_store_function(socket->session_, &SslDriver::_store);
    gnutls_db_set_remove_function(socket->session_, &SslDriver::_remove);
    gnutls_db_set_retrieve_function(socket->session_, &SslDriver::_retrieve);
}

// {{{ session cache
bool SslDriver::store(const gnutls_datum_t& key, const gnutls_datum_t& value)
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

gnutls_datum_t SslDriver::retrieve(const gnutls_datum_t& key) const
{
    gnutls_datum_t result = { nullptr, 0 };

    for (auto i = items_, e = i + size_; i != e; ++i) {
        if (i->equals(key)) {
            result.data = (unsigned char *)gnutls_malloc(i->value_size_);

            if (result.data) {
                std::memcpy(result.data, i->value_, i->value_size_);
                result.size = i->value_size_;
            }
            break;
        }
    }

    return result;
}

bool SslDriver::remove(gnutls_datum_t key)
{
    for (auto i = items_, e = i + size_; i != e; ++i) {
        if (i->equals(key)) {
            i->reset();
            return true;
        }
    }

    return false;
}

int SslDriver::_store(void *dbf, gnutls_datum_t key, gnutls_datum_t value)
{
    if (reinterpret_cast<SslDriver*>(dbf)->store(key, value))
        return 0;

    return -1;
}

gnutls_datum_t SslDriver::_retrieve(void *dbf, gnutls_datum_t key)
{
    return reinterpret_cast<SslDriver*>(dbf)->retrieve(key);
}

int SslDriver::_remove(void *dbf, gnutls_datum_t key)
{
    if (reinterpret_cast<SslDriver*>(dbf)->remove(key))
        return 0;

    return -1;
}
// }}}
