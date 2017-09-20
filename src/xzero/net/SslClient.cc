// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#if 1 == 0
#include <xzero/net/SslClient.h>

#include <xzero/io/FileUtil.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/tls1.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

namespace xzero {

Future<RefPtr<SslClient>> SslClient::connect(
    const InetAddress& target,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor,
    const std::string& sni,
    const std::vector<std::string>& applicationProtocolsSupported,
    std::function<Connection*(const std::string&)> createApplicationConnection) {

  return TcpUtil::connect(target, connectTimeout, executor).
      chain(std::bind(&SslClient::start,
                      std::placeholders::_1,
                      target.family(),
                      readTimeout, writeTimeout, executor, sni,
                      applicationProtocolsSupported,
                      createApplicationConnection));
}

Future<RefPtr<SslClient>> SslClient::start(
    FileDescriptor&& fd,
    int addressFamily,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor,
    const std::string& sni,
    const std::vector<std::string>& applicationProtocolsSupported,
    std::function<Connection*(const std::string&)> createApplicationConnection) {

  RefPtr<SslClient> sslClient = make_ref<SslClient>(
        std::move(fd), addressFamily,
        readTimeout, writeTimeout, executor, sni,
        applicationProtocolsSupported,
        createApplicationConnection);

  return sslClient->handshake();
}

Future<RefPtr<SslClient>> SslClient::handshake() {
  ref(); // XXX guards the object while being in handshake-mode
  Promise<RefPtr<SslClient>> promise;
  onHandshake(promise);
  return promise.future();
}

void SslClient::onHandshake(Promise<RefPtr<SslClient>> promise) {
  int rv = SSL_connect(ssl_);
  if (rv > 0) {
    if (X509* cert = SSL_get_peer_certificate(ssl_)) {
      // ...
      X509_free(cert);
    }

    promise.success(RefPtr<SslClient>(this));
    unref();
  } else {
    switch (SSL_get_error(ssl_, rv)) {
      case SSL_ERROR_WANT_READ:
        executor_->executeOnReadable(fd_, std::bind(&SslClient::onHandshake, this, promise));
        break;
      case SSL_ERROR_WANT_WRITE:
        executor_->executeOnWritable(fd_, std::bind(&SslClient::onHandshake, this, promise));
        break;
      case SSL_ERROR_SYSCALL:
      case SSL_ERROR_SSL:
        promise.failure(makeSslError(ERR_get_error()));
        unref();
        break;
      default:
        promise.failure(std::errc::invalid_argument); // weird things going on
        unref();
        break;
    }
  }
}

std::string SslClient::nextProtocolNegotiated() const {
  const unsigned char* data = nullptr;
  unsigned int len = 0;

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation // ALPN
  SSL_get0_alpn_selected(ssl_, &data, &len);
  if (len > 0) {
    return std::string((const char*) data, len);
  }
#endif

#ifdef TLSEXT_TYPE_next_proto_neg // NPN
  SSL_get0_next_proto_negotiated(ssl_, &data, &len);
  if (len > 0) {
    return std::string((const char*) data, len);
  }
#endif

  return std::string();
}

SslClient::SslClient(
    FileDescriptor&& fd,
    int addressFamily,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor,
    const std::string& sni,
    const std::vector<std::string>& applicationProtocolsSupported,
    std::function<Connection*(const std::string&)> createApplicationConnection)
    : method_(TLSv1_2_client_method()),
      ctx_(SSL_CTX_new(method_)),
      ssl_(SSL_new(ctx_)),
      fd_(std::move(fd)),
      addressFamily_(addressFamily),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      executor_(executor),
      createApplicationConnection_(createApplicationConnection),
      io_() {

  //SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, verify_callback);
  //SSL_CTX_set_verify_depth(ctx_, 4);
  //SSL_CTX_load_verify_locations(ctx_, "random-org-chain.pem", NULL);

  std::string protos = StringUtil::join(applicationProtocolsSupported, ":");
  SSL_CTX_set_alpn_protos(ctx_, (unsigned char*) protos.data(), protos.size());

  SSL_set_cipher_list(ssl_, "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4");
  SSL_set_tlsext_host_name(ssl_, sni.c_str());

  const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
  SSL_CTX_set_options(ctx_, flags);

  SSL_set_fd(ssl_, fd_);
}

SslClient::~SslClient() {
  SSL_free(ssl_);
  SSL_CTX_free(ctx_);
}

bool SslClient::isOpen() const {
  return fd_ >= 0;
}

void SslClient::close() {
}

std::string makeSslErrorString(int ec) {
  char buf[1024];
  buf[0] = '\0';
  ERR_error_string_n(ec, buf, sizeof(buf));
  return buf;
}

size_t SslClient::fill(Buffer* sink, size_t count) {
  for (;;) {
    int n = SSL_read(ssl_, sink->end(), count);
    if (n >= 0) {
      return n;
    }

    int err = SSL_get_error(ssl_, n);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
      logError("SslClient", "SSL_read error $0", makeSslErrorString(err));
      errno = EIO;
      return -1;
    }
  }
}

size_t SslClient::flush(const BufferRef& source) {
  for (;;) {
    int n = SSL_write(ssl_, source.data(), source.size());
    if (n >= 0) {
      return n;
    }

    int err = SSL_get_error(ssl_, n);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
      logError("SslClient", "SSL_write error $0", makeSslErrorString(err));
      errno = EIO;
      return -1;
    }
  }
}

size_t SslClient::flush(const FileView& fileView) {
  Buffer buf;
  FileUtil::read(fileView, &buf);
  return flush(buf);
}

void SslClient::wantFill() {
  if (io_)
    return;

  io_ = executor_->executeOnReadable(fd_,
      std::bind(&SslClient::fillable, this),
      readTimeout_,
      std::bind(&SslClient::onTimeout, this));
}

void SslClient::wantFlush() {
  if (io_)
    return;

  io_ = executor_->executeOnWritable(fd_,
      std::bind(&SslClient::flushable, this),
      writeTimeout_,
      std::bind(&SslClient::onTimeout, this));
}

Duration SslClient::readTimeout() {
  return readTimeout_;
}

Duration SslClient::writeTimeout() {
  return writeTimeout_;
}

void SslClient::setReadTimeout(Duration timeout) {
  readTimeout_ = timeout;
}

void SslClient::setWriteTimeout(Duration timeout) {
  writeTimeout_ = timeout;
}

bool SslClient::isBlocking() const {
  return FileUtil::isBlocking(fd_);
}

void SslClient::setBlocking(bool enable) {
  FileUtil::setBlocking(fd_, enable);
}

bool SslClient::isCorking() const {
  return TcpUtil::isCorking(fd_);
}

void SslClient::setCorking(bool enable) {
  TcpUtil::setCorking(fd_, enable);
}

bool SslClient::isTcpNoDelay() const {
  return TcpUtil::isTcpNoDelay(fd_);
}

void SslClient::setTcpNoDelay(bool enable) {
  TcpUtil::setTcpNoDelay(fd_, enable);
}

std::string SslClient::toString() const {
  return "SslClient";
}

Option<InetAddress> SslClient::remoteAddress() const {
  Result<InetAddress> addr = TcpUtil::getRemoteAddress(fd_, addressFamily_);
  if (addr.isSuccess())
    return Some(*addr);
  else
    return None();
}

Option<InetAddress> SslClient::localAddress() const {
  Result<InetAddress> addr = TcpUtil::getLocalAddress(fd_, addressFamily_);
  if (addr.isSuccess())
    return Some(*addr);
  else
    return None();
}

} // namespace xzero
#endif
