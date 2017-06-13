// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <string>
#include <openssl/ssl.h>

namespace xzero {

class SslConnector;

/**
 * An SSL context (certificate & keyfile).
 */
class XZERO_BASE_API SslContext {
 public:
  SslContext(SslConnector* connector,
             const std::string& crtFile,
             const std::string& keyFile);
  ~SslContext();

  SSL_CTX* get() const;

  const std::vector<std::string>& dnsNames() const;

  bool isValidDnsName(const std::string& servername) const;

 private:
  static bool imatch(const std::string& pattern, const std::string& value);
  static int onServerName(SSL* ssl, int* ad, SslContext* self);
  static int onNextProtosAdvertised(SSL* ssl,
      const unsigned char** out, unsigned int* outlen, void* pself);
  static int onAppLayerProtoNegotiation(SSL* ssl,
      const unsigned char **out, unsigned char *outlen,
      const unsigned char *in, unsigned int inlen, void *pself);

 private:
  SslConnector* connector_;
  SSL_CTX* ctx_;
  std::vector<std::string> dnsNames_;
};

inline SSL_CTX* SslContext::get() const {
  return ctx_;
}

inline const std::vector<std::string>& SslContext::dnsNames() const {
  return dnsNames_;
}

} // namespace xzero
