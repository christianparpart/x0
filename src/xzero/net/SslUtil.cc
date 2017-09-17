// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/SslUtil.h>
#include <xzero/net/SslConnector.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/BufferUtil.h>
#include <xzero/Buffer.h>
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
                                    int addressFamily,
                                    SslConnector* connector,
                                    ConnectionFactory connectionFactory,
                                    Executor* executor) {
  return accept(std::move(fd),
                addressFamily,
                connector->readTimeout(),
                connector->writeTimeout(),
                connector->defaultContext(),
                std::bind(&InetConnector::onEndPointClosed, connector, std::placeholders::_1),
                connectionFactory,
                executor);
}

RefPtr<SslEndPoint> SslUtil::accept(FileDescriptor&& fd,
                                    int addressFamily,
                                    Duration readTimeout,
                                    Duration writeTimeout,
                                    SslContext* defaultContext,
                                    std::function<void(EndPoint*)> onEndPointClosed,
                                    ConnectionFactory connectionFactory,
                                    Executor* executor) {
  return make_ref<SslEndPoint>(std::move(fd),
                               addressFamily,
                               readTimeout,
                               writeTimeout,
                               defaultContext,
                               connectionFactory,
                               onEndPointClosed,
                               executor);
}

Buffer SslUtil::makeProtocolList(const std::list<std::string>& protos) {
  Buffer out;

  size_t capacity = 0;
  for (const auto& proto: protos) {
    capacity += proto.size() + 1;
  }
  out.reserve(capacity);

  BinaryWriter writer(BufferUtil::writer(&out));
  for (const auto& proto: protos) {
    assert(proto.size() < 0xFF);
    writer.writeString(proto);
  }

  return out;
}

}  // namespace xzero
