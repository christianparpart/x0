// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/SslContext.h>
#include <xzero/net/SslEndPoint.h> // for makeProtocolList, SslErrorCategory
#include <xzero/net/TcpConnection.h>
#include <xzero/util/BinaryWriter.h>
#include <xzero/BufferUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/tls1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/objects.h>
#include <openssl/opensslconf.h>
#include <algorithm>

namespace xzero {

#define THROW_SSL_ERROR() {                                                   \
  RAISE_CATEGORY(ERR_get_error(), SslErrorCategory::get());                   \
}

// {{{ helper
static std::vector<std::string> collectDnsNames(X509* crt) {
  std::vector<std::string> result;

  auto addUnique = [&](const std::string& name) {
    auto i = std::find(result.begin(), result.end(), name);
    if (i == result.end()) {
      result.push_back(name);
    }
  };

  // retrieve DNS-Name extension entries
  STACK_OF(GENERAL_NAME)* altnames =
      (STACK_OF(GENERAL_NAME)*) X509_get_ext_d2i(crt, NID_subject_alt_name,
                                                 nullptr, nullptr);
  int numAltNames = sk_GENERAL_NAME_num(altnames);

  for (int i = 0; i < numAltNames; ++i) {
    GENERAL_NAME* altname = sk_GENERAL_NAME_value(altnames, i);
    if (altname->type == GEN_DNS) {
      std::string dnsName(reinterpret_cast<char*>(altname->d.dNSName->data),
                          altname->d.dNSName->length);
      addUnique(dnsName);
    }
  }
  GENERAL_NAMES_free(altnames);

  // Retrieve "commonName" subject
  X509_NAME* sname = X509_get_subject_name(crt);
  if (sname) {
    int i = -1;
    for (;;) {
      i = X509_NAME_get_index_by_NID(sname, NID_commonName, i);
      if (i < 0)
        break;

      X509_NAME_ENTRY* entry = X509_NAME_get_entry(sname, i);
      ASN1_STRING* str = X509_NAME_ENTRY_get_data(entry);

      std::string cn((char*)ASN1_STRING_data(str), ASN1_STRING_length(str));

      addUnique(cn);
    }
  }

  return result;
}

static std::vector<std::string> collectDnsNames(SSL_CTX* ctx) {
  SSL* ssl = SSL_new(ctx);
  X509* crt = SSL_get_certificate(ssl);
  std::vector<std::string> result = collectDnsNames(crt);
  SSL_free(ssl);
  return result;
}
// }}}

SslContext::SslContext(const std::string& crtFilePath,
                       const std::string& keyFilePath,
                       const BufferRef& alpn,
                       std::function<SslContext*(const char*)> getContext)
    : ctx_(nullptr),
      alpn_(alpn),
      dnsNames_(),
      getContext_(getContext) {
  initialize();

#if defined(SSL_TXT_TLSV1_2)
  ctx_ = SSL_CTX_new(TLSv1_2_server_method());
#elif defined(SSL_TXT_TLSV1_1)
  ctx_ = SSL_CTX_new(TLSv1_1_server_method());
#else
  ctx_ = SSL_CTX_new(TLSv1_server_method());
#endif

  if (!ctx_)
    THROW_SSL_ERROR();

  if (SSL_CTX_use_certificate_chain_file(ctx_, crtFilePath.c_str()) <= 0)
    THROW_SSL_ERROR();

  if (SSL_CTX_use_PrivateKey_file(ctx_, keyFilePath.c_str(), SSL_FILETYPE_PEM) <= 0)
    THROW_SSL_ERROR();

  if (!SSL_CTX_check_private_key(ctx_))
    throw SslPrivateKeyError{};

  SSL_CTX_set_tlsext_servername_callback(ctx_, &SslContext::onServerName);
  SSL_CTX_set_tlsext_servername_arg(ctx_, this);

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
  SSL_CTX_set_alpn_select_cb(ctx_, &SslContext::onAppLayerProtoNegotiation, this);
#endif

  dnsNames_ = collectDnsNames(ctx_);
}

SslContext::~SslContext() {
  SSL_CTX_free(ctx_);
}

int SslContext::onAppLayerProtoNegotiation(
    SSL* ssl,
    const unsigned char **out, unsigned char *outlen,
    const unsigned char *in, unsigned int inlen,
    void *pself) {
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation

  for (unsigned int i = 0; i < inlen; i += in[i] + 1) {
    std::string proto((char*)&in[i + 1], in[i]);
  }

  SslContext* self = (SslContext*) pself;

  if (SSL_select_next_proto((unsigned char**) out, outlen,
                            (unsigned char*) self->alpn_.data(), self->alpn_.size(),
                            in, inlen) != OPENSSL_NPN_NEGOTIATED) {
    return SSL_TLSEXT_ERR_NOACK;
  }

  return SSL_TLSEXT_ERR_OK;
#else
  return SSL_TLSEXT_ERR_NOACK;
#endif
}

int SslContext::onServerName(SSL* ssl, int* ad, SslContext* self) {
  const char* name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
  if (!name) {
    if (SslContext* ctx = self->getContext_(nullptr)) {
      SSL_set_SSL_CTX(ssl, ctx->get());
      return SSL_TLSEXT_ERR_OK;
    } else {
      return SSL_TLSEXT_ERR_NOACK;
    }
  }

  if (SslContext* ctx = self->getContext_(name)) {
    SSL_set_SSL_CTX(ssl, ctx->get());
    return SSL_TLSEXT_ERR_OK;
  }

  return SSL_TLSEXT_ERR_NOACK;
}

bool SslContext::isValidDnsName(const std::string& servername) const {
  for (const std::string& pattern: dnsNames_)
    if (imatch(pattern, servername))
      return true;

  return false;
}

bool SslContext::imatch(const std::string& pattern, const std::string& value) {
  int s = pattern.size() - 1;
  int t = value.size() - 1;

  for (; s > 0 && t > 0 && pattern[s] == value[t]; --s, --t)
    ;

  if (!s && !t)
    return true;

  if (pattern[s] == '*')
    return true;

  return false;
}

void SslContext::initialize() {
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
#if !defined(OPENSSL_THREADS)
  logDebug("OpenSSL implementation has no thread-locking support");
#endif
}

} // namespace xzero
