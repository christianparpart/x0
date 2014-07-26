// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef sw_x0_SslContext_h
#define sw_x0_SslContext_h (1)

#include <base/Property.h>
#include <base/LogMessage.h>
#include <base/Logger.h>
#include <base/Types.h>

#include <string>
#include <vector>
#include <system_error>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

class SslSocket;

class SslContext {
 public:
  SslContext();
  ~SslContext();

  void setLogger(base::Logger* logger);

  template <typename... Args>
  void log(base::Severity severity, const char* fmt, Args... args);

  void setCertFile(const std::string& filename);
  void setKeyFile(const std::string& filename);
  void setCrlFile(const std::string& filename);
  void setTrustFile(const std::string& filename);

  std::string commonName() const;

  bool isValidDnsName(const std::string& dnsName) const;

  bool post_config();

  void bind(SslSocket* socket);

  friend class SslSocket;

 public:
  bool enabled;
  base::WriteProperty<std::string, SslContext, &SslContext::setCertFile> certFile;
  base::WriteProperty<std::string, SslContext, &SslContext::setKeyFile> keyFile;
  base::WriteProperty<std::string, SslContext, &SslContext::setCrlFile> crlFile;
  base::WriteProperty<std::string, SslContext, &SslContext::setTrustFile>
  trustFile;

  static int onRetrieveCert(gnutls_session_t session, gnutls_retr_st* ret);

 private:
  static bool imatch(const std::string& pattern, const std::string& value);

  std::error_code error_;
  base::Logger* logger_;

  // GNU TLS specific properties
  gnutls_certificate_credentials_t certs_;
  gnutls_anon_server_credentials_t anonCreds_;
  gnutls_srp_server_credentials_t srpCreds_;
  std::string certCN_;
  std::vector<std::string> dnsNames_;

  // x509
  gnutls_x509_privkey_t x509PrivateKey_;
  gnutls_x509_crt_t x509Certs_[8];
  unsigned numX509Certs_;
  gnutls_certificate_request_t clientVerifyMode_;

  // OpenPGP
  gnutls_openpgp_crt_t pgpCert_;  // a chain, too.
  gnutls_openpgp_privkey_t pgpPrivateKey_;

  // general
  gnutls_rsa_params_t rsaParams_;
  gnutls_dh_params_t dhParams_;
  gnutls_x509_crt_t* caList_;
};

// {{{ inlines
template <typename... Args>
void SslContext::log(base::Severity severity, const char* fmt, Args... args) {
  if (logger_) {
    base::LogMessage msg(severity, fmt, args...);
    logger_->write(msg);
  }
}

inline bool SslContext::isValidDnsName(const std::string& dnsName) const {
  if (imatch(commonName(), dnsName)) return true;

  for (auto i = dnsNames_.begin(), e = dnsNames_.end(); i != e; ++i)
    if (imatch(*i, dnsName)) return true;

  return false;
}

inline bool SslContext::imatch(const std::string& pattern,
                               const std::string& value) {
  int s = pattern.size() - 1;
  int t = value.size() - 1;

  for (; s > 0 && t > 0 && pattern[s] == value[t]; --s, --t)
    ;

  if (!s && !t) return true;

  if (pattern[s] == '*') return true;

  return false;
}
// }}}

class SslContextSelector {
 public:
  virtual ~SslContextSelector() {}

  virtual SslContext* select(const std::string& dnsName) const = 0;
};

#endif
