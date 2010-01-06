/* <x0/mod_ssl.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include <gnutls/gnutls.h>
#include <gnutls/extra.h>
#include <pthread.h>
#include <gcrypt.h>
GCRY_THREAD_OPTION_PTHREAD_IMPL;

#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define SSL_DEBUG(msg...) printf("ssl: " msg)

/**
 * \ingroup plugins
 * \brief implements ssl maps, mapping request paths to custom local paths (overriding resolved document_root concatation)
 */
class ssl_plugin :
	public x0::plugin
{
private:
	boost::signals::connection c;

	typedef std::map<std::string, std::string> sslmap_type;

	struct context
	{
		sslmap_type ssles;
	};

	gnutls_priority_t priority_cache_;
	gnutls_certificate_credentials_t x509_cred_;
	gnutls_dh_params_t dh_params_;

public:
	ssl_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

		int rv = gnutls_global_init();
		if (rv != GNUTLS_E_SUCCESS)
			throw std::runtime_error("could not initialize gnutls library");

		gnutls_global_init_extra();

		server_.connection_open.connect(boost::bind(&ssl_plugin::connection_open, this, _1, _2));
		server_.connection_close.connect(boost::bind(&ssl_plugin::connection_close, this, _1));
	}

	static void gnutls_log_cb(int level, const char *msg)
	{
		SSL_DEBUG("gnutls: %s", msg);
	}

	~ssl_plugin()
	{
		server_.free_context<context>(this);

		gnutls_global_deinit();
	}

	virtual void configure()
	{
		// gnutls debug level (int, 0=off)
		gnutls_global_set_log_level(10);
		gnutls_global_set_log_function(&ssl_plugin::gnutls_log_cb);

		// setup x509
		gnutls_certificate_allocate_credentials(&x509_cred_);
		gnutls_certificate_set_x509_trust_file(x509_cred_, "ca.pem", GNUTLS_X509_FMT_PEM);
		gnutls_certificate_set_x509_crl_file(x509_cred_, "crl.pem", GNUTLS_X509_FMT_PEM);
		gnutls_certificate_set_x509_key_file(x509_cred_, "cert.pem", "key.pem", GNUTLS_X509_FMT_PEM);

		// setup dh params
		gnutls_dh_params_init(&dh_params_);
		gnutls_dh_params_generate2(dh_params_, 1024);
	}

private:
	void connection_open(const boost::function<void()>& completed, x0::connection_ptr conn)
	{
		if (!ssl_enabled(conn))
			return;

		SSL_DEBUG("open()");

		gnutls_session_t session = create_ssl_session();

		int rv = gnutls_handshake(session);
		if (rv == GNUTLS_E_AGAIN || rv == GNUTLS_E_INTERRUPTED)
			; /// \todo notify core that we need to be invoked again (once data-read becomes ready && before http protocol layer starts processing)

		gnutls_transport_set_ptr(session, reinterpret_cast<void *>(conn->socket().native()));

		/// \todo hook into connection's read*() / write*() functions

		completed();
	}

	void connection_close(x0::connection *conn)
	{
//		if (ssl_enabled(conn))
//			return;

//		gnutls_bye(session);

		/// \todo cleanup
		SSL_DEBUG("close()");
	}

private:
	bool ssl_enabled(x0::connection_ptr conn)
	{
		/// \todo ssl_enabled(conn)
		return true;
	}

	gnutls_session_t create_ssl_session()
	{
		gnutls_session_t session;

		gnutls_init(&session, GNUTLS_SERVER);
		gnutls_priority_set(session, priority_cache_);
		gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, x509_cred_);
		gnutls_certificate_server_set_request(session, GNUTLS_CERT_REQUEST);
		gnutls_session_enable_compatibility_mode(session);

		return session;
	}
};

X0_EXPORT_PLUGIN(ssl);
