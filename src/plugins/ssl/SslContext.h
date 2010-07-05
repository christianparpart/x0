/* "SslContex"
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2010 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_SslContext_h
#define sw_x0_SslContext_h (1)

#include <x0/Property.h>
#include <x0/Scope.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <string>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/extra.h>

class SslSocket;
class SslDriver;

struct SslContext :
	public x0::ScopeValue
{
public:
	SslContext();
	~SslContext();

	virtual void merge(const ScopeValue *from);

	void setDriver(SslDriver *driver);

	void setCertFile(const std::string& filename);
	void setKeyFile(const std::string& filename);
	void setCrlFile(const std::string& filename);
	void setTrustFile(const std::string& filename);
	void setPriorities(const std::string& value);

	std::string commonName() const;

	void post_config();

	void bind(SslSocket *socket);

	friend class SslSocket;

public:
	bool enabled;
	x0::WriteProperty<std::string, SslContext, &SslContext::setCertFile> certFile;
	x0::WriteProperty<std::string, SslContext, &SslContext::setKeyFile> keyFile;
	x0::WriteProperty<std::string, SslContext, &SslContext::setCrlFile> crlFile;
	x0::WriteProperty<std::string, SslContext, &SslContext::setTrustFile> trustFile;
	x0::WriteProperty<std::string, SslContext, &SslContext::setPriorities> priorities;

	static int onRetrieveCert(gnutls_session_t session, gnutls_retr_st *ret);

private:
	SslDriver *driver_;

	// GNU TLS specific properties
	gnutls_certificate_credentials_t certs_;
	gnutls_anon_server_credentials_t anonCreds_;
	gnutls_srp_server_credentials_t srpCreds_;
	std::string certCN_;

	// x509
	gnutls_x509_privkey_t x509PrivateKey_;
	gnutls_x509_crt_t x509Certs_[8];
	unsigned numX509Certs_;
	gnutls_certificate_request_t clientVerifyMode_;

	// OpenPGP
	gnutls_openpgp_crt_t pgpCert_; // a chain, too.
	gnutls_openpgp_privkey_t pgpPrivateKey_;

	// general
	gnutls_priority_t priorities_;
	gnutls_rsa_params_t rsaParams_;
	gnutls_dh_params_t dhParams_;
	gnutls_x509_crt_t *caList_;
};

class SslContextSelector
{
public:
	virtual ~SslContextSelector() {}

	virtual SslContext *select(const std::string& dnsName) const = 0;
};

#endif
