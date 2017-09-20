// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/SslUtil.h>
#include <xzero/net/SslConnector.h>
#include <xzero/net/SslContext.h>
#include <xzero/net/Connection.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <xzero/RuntimeError.h>
#include <openssl/ssl.h>
#include <algorithm>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("SslConnector", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

SslConnector::SslConnector(const std::string& name, Executor* executor,
                           ExecutorSelector clientExecutorSelector,
                           Duration readTimeout, Duration writeTimeout,
                           Duration tcpFinTimeout,
                           const IPAddress& ipaddress, int port, int backlog,
                           bool reuseAddr, bool reusePort)
    : TcpConnector(name, executor, clientExecutorSelector,
                   readTimeout, writeTimeout, tcpFinTimeout,
                   ipaddress, port, backlog, reuseAddr, reusePort),
      protocolList_(),
      contexts_() {
}

SslConnector::~SslConnector() {
}

void SslConnector::addConnectionFactory(const std::string& protocol,
                                        ConnectionFactory factory) {
  TcpConnector::addConnectionFactory(protocol, factory);

  // XXX needs update whenever a new protocol-implementation is added.
  // XXX should only happen at startup-time, too
  protocolList_ = SslUtil::makeProtocolList(connectionFactories());
}

void SslConnector::addContext(const std::string& crtFilePath,
                              const std::string& keyFilePath) {
  contexts_.emplace_back(new SslContext(this, crtFilePath, keyFilePath));
}

SslContext* SslConnector::selectContext(const char* servername) const {
  TRACE("$0 selectContext: servername = '$1'", this, servername);
  if (!servername)
    return nullptr;

  for (const auto& ctx: contexts_)
    if (ctx->isValidDnsName(servername))
      return ctx.get();

  return nullptr;
}

int SslConnector::selectContext(
    SSL* ssl, int* ad, SslConnector* self) {
  const char * servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
  TRACE("$0 selectContext: servername = '$1'", self, servername);

  if (!servername)
    return SSL_TLSEXT_ERR_NOACK;

  for (const auto& ctx: self->contexts_) {
    if (ctx->isValidDnsName(servername)) {
      TRACE("selecting context $0", (void*) ctx->get());
      SSL_set_SSL_CTX(ssl, ctx->get());
      return SSL_TLSEXT_ERR_OK;
    }
  }

  TRACE("using default context $0", (void*) SSL_get_SSL_CTX(ssl));
  return SSL_TLSEXT_ERR_OK;
}

RefPtr<TcpEndPoint> SslConnector::createEndPoint(int cfd, Executor* executor) {
  TRACE("createEndPoint: cfd=$0", cfd);
  return make_ref<SslEndPoint>(
      FileDescriptor(cfd),
      addressFamily(),
      readTimeout(),
      writeTimeout(),
      defaultContext(),
      std::bind(&SslConnector::createConnection, this, std::placeholders::_1, std::placeholders::_2),
      std::bind(&SslConnector::onEndPointClosed, this, std::placeholders::_1),
      executor).as<TcpEndPoint>();
}

void SslConnector::onEndPointCreated(RefPtr<TcpEndPoint> endpoint) {
  TRACE("onEndPointCreated fd=$0", endpoint->handle());
  endpoint.weak_as<SslEndPoint>()->onHandshake();
}

} // namespace xzero
