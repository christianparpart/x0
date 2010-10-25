/* <SslContext.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_SslContext_h
#define sw_x0_SslContext_h (1)

#include <x0/Property.h>
#include <x0/Logger.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <system_error>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/extra.h>

class SslSocket;

struct SslContext
{
public:
	SslContext();
	~SslContext();

	void setLogger(x0::Logger *logger);

	void setCertFile(const std::string& filename);
	void setKeyFile(const std::string& filename);
	void setCrlFile(const std::string& filename);
	void setTrustFile(const std::string& filename);
	void setPriorities(const std::string& value);

	std::string commonName() const;

	bool isValidDnsName(const std::string& dnsName) const;

	bool post_config();

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
	static bool imatch(const std::string& pattern, const std::string& value);

	std::error_code error_;
	x0::Logger *logger_;

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
	gnutls_openpgp_crt_t pgpCert_; // a chain, too.
	gnutls_openpgp_privkey_t pgpPrivateKey_;

	// general
	gnutls_priority_t priorities_;
	gnutls_rsa_params_t rsaParams_;
	gnutls_dh_params_t dhParams_;
	gnutls_x509_crt_t *caList_;
};

// {{{ inlines
inline bool SslContext::isValidDnsName(const std::string& dnsName) const
{
	if (imatch(commonName(), dnsName))
		return true;

	for (auto i = dnsNames_.begin(), e = dnsNames_.end(); i != e; ++i)
		if (imatch(*i, dnsName))
			return true;

	return false;
}

inline bool SslContext::imatch(const std::string& pattern, const std::string& value)
{
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
// }}}

class SslContextSelector
{
public:
	virtual ~SslContextSelector() {}

	virtual SslContext *select(const std::string& dnsName) const = 0;
};

#endif
