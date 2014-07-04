// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_SslDriver_h
#define sw_x0_SslDriver_h (1)

#include <x0/SocketDriver.h>
#include "SslContext.h"  // SslContextSelector
#include "SslSocket.h"
#include <gnutls/gnutls.h>

struct SslCacheItem;
class SslContext;

class SslDriver : public x0::SocketDriver {
 private:
  gnutls_priority_t priorities_;

  SslContextSelector *selector_;

  SslCacheItem *items_;
  std::size_t size_;
  std::size_t ptr_;

  friend class SslSocket;
  friend class SslContext;

 public:
  SslDriver(SslContextSelector *selector);
  virtual ~SslDriver();

  virtual bool isSecure() const;

  virtual SslSocket *create(struct ev_loop *loop, int handle, int af);
  virtual void destroy(x0::Socket *);

  void setPriorities(const std::string &value);

  void initialize(SslSocket *socket);

  SslContext *selectContext(const std::string &dnsName) const;

  const SslContextSelector *selector() const;

 private:
  bool store(const gnutls_datum_t &key, const gnutls_datum_t &value);
  gnutls_datum_t retrieve(const gnutls_datum_t &key) const;
  bool remove(gnutls_datum_t key);

  static int _store(void *dbf, gnutls_datum_t key, gnutls_datum_t value);
  static gnutls_datum_t _retrieve(void *dbf, gnutls_datum_t key);
  static int _remove(void *dbf, gnutls_datum_t key);
};

// {{{ inlines
inline const SslContextSelector *SslDriver::selector() const {
  return selector_;
}
// }}}

#endif
