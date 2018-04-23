// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/sysconfig.h>
#include <xzero/defines.h>
#include <xzero/Buffer.h>
#include <string>
#include <functional>
#include <stdexcept>

#include <openssl/ssl.h>

namespace xzero {

class SslPrivateKeyError : public std::runtime_error {
 public:
  SslPrivateKeyError() : std::runtime_error("SSL private key error.") {}
};

/**
 * An SSL context (certificate & keyfile).
 */
class SslContext {
 public:
  SslContext(const std::string& crtFile,
             const std::string& keyFile,
             const BufferRef& alpn,
             std::function<SslContext*(const char*)> getContext);

  ~SslContext();

  const std::vector<std::string>& dnsNames() const;

  bool isValidDnsName(const std::string& servername) const;

  static void initialize();

  SSL_CTX* get() const;

private:
  static bool imatch(const std::string& pattern, const std::string& value);

  static int onServerName(SSL* ssl, int* ad, SslContext* self);
  static int onAppLayerProtoNegotiation(
      SSL* ssl,
      const unsigned char **out, unsigned char *outlen,
      const unsigned char *in, unsigned int inlen,
      void *pself);

 private:
  SSL_CTX* ctx_;

  BufferRef alpn_;

  std::vector<std::string> dnsNames_;
  std::function<SslContext*(const char*)> getContext_;
};

inline SSL_CTX* SslContext::get() const {
  return ctx_;
}

inline const std::vector<std::string>& SslContext::dnsNames() const {
  return dnsNames_;
}

} // namespace xzero
