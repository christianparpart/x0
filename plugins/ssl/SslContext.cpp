/* <x0/plugins/ssl/SslContext.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include "SslContext.h"
#include "SslSocket.h"
#include "SslDriver.h"
#include <x0/strutils.h>
#include <x0/Types.h>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#if 0
#	define TRACE(msg...) DEBUG("SslContext: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif

std::error_code loadFile(gnutls_datum_t& data, const std::string& filename) // {{{ loadFile / freeFile
{
	//TRACE("loadFile('%s')", filename.c_str());
	struct stat st;
	if (stat(filename.c_str(), &st) < 0)
		return std::make_error_code(static_cast<std::errc>(errno));

	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0)
		return std::make_error_code(static_cast<std::errc>(errno));;

	data.data = new unsigned char[st.st_size + 1];
	data.size = read(fd, data.data, st.st_size);

	/*if (data.size < st.st_size)
	{
		delete[] data.data;
		TRACE("loadFile: read %d (of %ld) bytes [failed: %s]", data.size, st.st_size, strerror(errno));
		return false;
	}*/

	close(fd);

	data.data[data.size] = '\0';
	//TRACE("loadFile: read %d bytes (%ld)", data.size, strlen((char *)data.data));
	//TRACE("dump(%s)", data.data);
	return std::error_code();
}

void freeFile(gnutls_datum_t data)
{
	delete[] data.data;
} // }}}

SslContext::SslContext() :
	enabled(true),
	certFile(this),
	keyFile(this),
	crlFile(this),
	trustFile(this),
	priorities(this),
	error_(),
	logger_(0),
	numX509Certs_(0),
	clientVerifyMode_(GNUTLS_CERT_IGNORE),
	caList_(0)
{
	TRACE("SslContext()");

	gnutls_dh_params_init(&dhParams_);
	gnutls_dh_params_generate2(dhParams_, 1024);

	gnutls_certificate_allocate_credentials(&certs_);
	gnutls_anon_allocate_server_credentials(&anonCreds_);
}

SslContext::~SslContext()
{
	TRACE("~SslContext()");

	if (!priorities().empty())
		gnutls_priority_deinit(priorities_);

	if (certs_)
		gnutls_certificate_free_credentials(certs_);
}

void SslContext::setLogger(x0::Logger *logger)
{
	logger_ = logger;
}

void SslContext::setCertFile(const std::string& filename)
{
	if (error_) return;
	if (!enabled) return;

	TRACE("SslContext::setCertFile: \"%s\"", filename.c_str());
	gnutls_datum_t data;
	error_ = loadFile(data, filename);
	if (error_) {
		logger_->write(x0::Severity::error, "Error loading certificate file(%s): %s", filename.c_str(), error_.message().c_str());
		return;
	}

	numX509Certs_ = sizeof(x509Certs_) / sizeof(*x509Certs_); // 8

	int rv;
	rv = gnutls_x509_crt_list_import(x509Certs_, &numX509Certs_, &data, GNUTLS_X509_FMT_PEM, 0);

#if !defined(NDEBUG)
	if (rv < 0) {
		TRACE("gnutls_x509_crt_list_import: \"%s\"", gnutls_strerror(rv));
	}
#endif

	for (unsigned i = 0; i < numX509Certs_; ++i)
	{
		// read Common Name (CN):
		std::size_t len = 0;
		rv = gnutls_x509_crt_get_dn_by_oid(x509Certs_[i], GNUTLS_OID_X520_COMMON_NAME, 0, 0, nullptr, &len);
		if (rv == GNUTLS_E_SHORT_MEMORY_BUFFER && len > 1)
		{
			char *buf = new char[len + 1];
			rv = gnutls_x509_crt_get_dn_by_oid(x509Certs_[i], GNUTLS_OID_X520_COMMON_NAME, 0, 0, buf, &len);
			certCN_ = buf;
			delete[] buf;
			TRACE("setCertFile: Common Name: \"%s\"", certCN_.c_str());
		}

		// read Subject Alternative-Name:
		for (int k = 0; !(rv < 0); ++k)
		{
			len = 0;
			rv = gnutls_x509_crt_get_subject_alt_name(x509Certs_[i], k, nullptr, &len, nullptr);
			if (rv == GNUTLS_E_SHORT_MEMORY_BUFFER && len > 1)
			{
				char *buf = new char[len + 1];
				rv = gnutls_x509_crt_get_subject_alt_name(x509Certs_[i], k, buf, &len, nullptr);
				buf[len] = '\0';

				if (rv == GNUTLS_SAN_DNSNAME)
					dnsNames_.push_back(buf);

				TRACE("setCertFile: Subject: \"%s\"", buf);
				delete[] buf;
			}
		}
	}

	freeFile(data);
	//TRACE("setCertFile: success.");
}

void SslContext::setKeyFile(const std::string& filename)
{
	if (error_) return;
	if (!enabled) return;

	TRACE("SslContext::setKeyFile: \"%s\"", filename.c_str());
	gnutls_datum_t data;
	if ((error_ = loadFile(data, filename))) {
		logger_->write(x0::Severity::error, "Error loading private key file(%s): %s", filename.c_str(), error_.message().c_str());
		return;
	}

	int rv;
	if ((rv = gnutls_x509_privkey_init(&x509PrivateKey_)) < 0) {
		freeFile(data);
		return;
	}

	if ((rv = gnutls_x509_privkey_import(x509PrivateKey_, &data, GNUTLS_X509_FMT_PEM)) < 0)
		rv = gnutls_x509_privkey_import_pkcs8(x509PrivateKey_, &data, GNUTLS_X509_FMT_PEM, nullptr, GNUTLS_PKCS_PLAIN);

	if (rv < 0) {
		freeFile(data);
		return;
	}

	freeFile(data);
	//TRACE("setKeyFile: success.");
}

void SslContext::setCrlFile(const std::string& filename)
{
	if (error_) return;
	if (!enabled) return;

	TRACE("setCrlFile");
}

void SslContext::setTrustFile(const std::string& filename)
{
	if (error_) return;
	if (!enabled) return;

	TRACE("setCrlFile");
}

void SslContext::setPriorities(const std::string& value)
{
	if (error_) return;
	if (!enabled) return;

	TRACE("setPriorities: \"%s\"", value.c_str());

	const char *errp = nullptr;
	int rv = gnutls_priority_init(&priorities_, value.c_str(), &errp);

	if (rv != GNUTLS_E_SUCCESS) {
		TRACE("gnutls_priority_init: error: %s \"%s\"", gnutls_strerror(rv), errp ? errp : "");
	}
}

std::string SslContext::commonName() const
{
	return certCN_;
}

bool SslContext::post_config()
{
	TRACE("SslContext.postConfig()\n");

	if (error_) return false;
	if (!enabled) return false;

	if (priorities().empty())
		priorities = "NORMAL";

//	if (rsaParams_)
//		gnutls_certificate_set_rsa_export_params(certs_, rsaParams_);

	if (dhParams_) {
		gnutls_certificate_set_dh_params(certs_, dhParams_);
		gnutls_anon_set_server_dh_params(anonCreds_, dhParams_);
	}

	gnutls_certificate_server_set_retrieve_function(certs_, &SslContext::onRetrieveCert);

	// SRP...

	return true;
}

int SslContext::onRetrieveCert(gnutls_session_t session, gnutls_retr_st *ret)
{
	TRACE("onRetrieveCert()");
	SslSocket *socket = (SslSocket *)gnutls_session_get_ptr(session);
	const SslContext *cx = socket->context();

	switch (gnutls_certificate_type_get(session)) {
		case GNUTLS_CRT_X509:
			if (!cx)
				return GNUTLS_E_INTERNAL_ERROR;

			ret->type = GNUTLS_CRT_X509;
			ret->deinit_all = 0;
			ret->ncerts = cx->numX509Certs_;
			ret->cert.x509 = const_cast<SslContext *>(cx)->x509Certs_;
			ret->key.x509 = cx->x509PrivateKey_;

			return GNUTLS_E_SUCCESS;
		case GNUTLS_CRT_OPENPGP:
			//return GNUTLS_E_SUCCESS;
		default:
			return GNUTLS_E_INTERNAL_ERROR;
	}
}

void SslContext::bind(SslSocket *socket)
{
	TRACE("bind() (cn=\"%s\")", commonName().c_str());

	socket->context_ = this;
	gnutls_certificate_server_set_request(socket->session_, clientVerifyMode_);
	gnutls_credentials_set(socket->session_, GNUTLS_CRD_CERTIFICATE, certs_);
	gnutls_credentials_set(socket->session_, GNUTLS_CRD_ANON, anonCreds_);
	gnutls_priority_set(socket->session_, priorities_);

	// XXX following function is marked deprecated and has no replacement API it seems.
	//const int cprio[] = { GNUTLS_CRT_X509, 0 };
	//gnutls_certificate_type_set_priority(socket->session_, cprio);
}
