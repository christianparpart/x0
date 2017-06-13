// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/SslUtil.h>
#include <xzero/net/SslConnector.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslconf.h>
#include <stdio.h>

namespace xzero {

void SslUtil::initialize() {
  static bool initialized = false;
  if (initialized)
    return;

  initialized = true;

  SSL_library_init();
  SSL_load_error_strings();
  //ERR_load_crypto_strings();
  //OPENSSL_config(NULL);
  OpenSSL_add_all_algorithms();

/* Include <openssl/opensslconf.h> to get this define */
#if defined(OPENSSL_THREADS)
  fprintf(stdout, "Warning: thread locking is not implemented\n");
#endif
}

std::error_code SslUtil::error(int ev) {
  return std::error_code(ev, SslErrorCategory::get());
}

std::error_category& SslErrorCategory::get() {
  static SslErrorCategory ec;
  return ec;
}

const char* SslErrorCategory::name() const noexcept {
  return "ssl";
}

std::string SslErrorCategory::message(int ev) const {
  char buf[256];
  ERR_error_string_n(ev, buf, sizeof(buf));
  return buf;
}


RefPtr<SslEndPoint> SslUtil::accept(FileDescriptor&& fd,
                                    SslConnector* connector,
                                    InetUtil::ConnectionFactory connectionFactory,
                                    Executor* executor) {
  return accept(std::move(fd),
                connector->readTimeout(),
                connector->writeTimeout(),
                connector->defaultContext(),
                std::bind(&InetConnector::onEndPointClosed, connector, std::placeholders::_1),
                connectionFactory,
                executor);
}

RefPtr<SslEndPoint> SslUtil::accept(FileDescriptor&& fd,
                                    Duration readTimeout,
                                    Duration writeTimeout,
                                    SslContext* defaultContext,
                                    std::function<void(EndPoint*)> onEndPointClosed,
                                    InetUtil::ConnectionFactory connectionFactory,
                                    Executor* executor) {
  return make_ref<SslEndPoint>(std::move(fd),
                               readTimeout,
                               writeTimeout,
                               defaultContext,
                               onEndPointClosed,
                               // connectionFactory,
                               executor);
}

}  // namespace xzero
