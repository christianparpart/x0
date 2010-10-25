/* <x0/plugins/ssl/ssl.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "SslContext.h"
#include "SslDriver.h"
#include "SslSocket.h"
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpListener.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <boost/lexical_cast.hpp>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/extra.h>
#include <pthread.h>
#include <gcrypt.h>
GCRY_THREAD_OPTION_PTHREAD_IMPL;

#if 0
#	define TRACE(msg...) DEBUG("ssl: " msg)
#else
#	define TRACE(msg...) /*!*/
#endif


/*
 * possible flow API:
 *
 *     void ssl.listen('IP:PORT');
 *     void ssl.listen('IP:PORT', backlog);
 *     void ssl.listen('IP:PORT', backlog, defaultKey, defaultCrt);
 *
 *     void ssl.add(hostname, certfile, keyfile);
 *
 *
 * EXAMPLE:
 *     ssl.listen '0.0.0.0:8443';
 *
 *     ssl.add 'hostname' => 'www.trapni.de',
 *             'certfile' => '/path/to/my.crt',
 *             'keyfile' => '/path/to/my.key',
 *             'crlfile' => '/path/to/my.crl';
 *
 */

/**
 * \ingroup plugins
 * \brief SSL plugin
 */
class ssl_plugin :
	public x0::HttpPlugin,
	public SslContextSelector
{
public:
	ssl_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		gcry_control(GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);

		int rv = gnutls_global_init();
		if (rv != GNUTLS_E_SUCCESS)
		{
			TRACE("gnutls_global_init: %s", gnutls_strerror(rv));
			return; //Error::CouldNotInitializeSslLibrary;
		}

		gnutls_global_init_extra();

		server().addComponent(std::string("GnuTLS/") + gnutls_check_version(NULL));

		registerSetupFunction<ssl_plugin, &ssl_plugin::add_listener>("ssl.listen", Flow::Value::VOID);
		registerSetupFunction<ssl_plugin, &ssl_plugin::add_context>("ssl.context", Flow::Value::VOID);
		registerSetupProperty<ssl_plugin, &ssl_plugin::set_loglevel>("ssl.loglevel", Flow::Value::VOID);
	}

	~ssl_plugin()
	{
		gnutls_global_deinit();
	}

	std::vector<SslContext *> contexts_;

	/** select the SSL context based on host name or NULL if nothing found. */
	virtual SslContext *select(const std::string& dnsName) const
	{
		if (dnsName.empty())
			return contexts_.front();

		for (auto i = contexts_.begin(), e = contexts_.end(); i != e; ++i)
		{
			SslContext *cx = *i;

			if (cx->isValidDnsName(dnsName))
			{
				TRACE("select SslContext: CN:%s, dnsName:%s", cx->commonName().c_str(), dnsName.c_str());
				return cx;
			}
		}

		return NULL;
	}

	virtual bool post_config()
	{
		for (auto i = contexts_.begin(), e = contexts_.end(); i != e; ++i)
			(*i)->post_config();

		return true;
	}

	virtual bool post_check()
	{
		// TODO do some post-config checks here.
		return true;
	}

	// {{{ config
private:
	// ssl.listener(BINDADDR_PORT);
	void add_listener(Flow::Value& result, const x0::Params& args)
	{
		std::string arg(args[0].toString());
		size_t n = arg.find(':');
		std::string ip = n != std::string::npos ? arg.substr(0, n) : "0.0.0.0";
		int port = atoi(n != std::string::npos ? arg.substr(n + 1).c_str() : arg.c_str());
		int backlog = args[1].isNumber() ? args[1].toNumber() : 0;

		x0::HttpListener *listener = server().setupListener(port, ip);

		if (listener && backlog)
			listener->backlog(backlog);

		SslDriver *driver = new SslDriver(this);
		listener->setSocketDriver(driver);
	}

	void set_loglevel(Flow::Value& result, const x0::Params& args)
	{
		if (args.count() == 1)
		{
			if (args[0].isNumber())
				setLogLevel(args[0].toNumber());
		}
	}

	void setLogLevel(int value)
	{
		value = std::max(-10, std::min(10, value));
		TRACE("setLogLevel: %d", value);

		gnutls_global_set_log_level(value);
		gnutls_global_set_log_function(&ssl_plugin::gnutls_logger);
	}

	static void gnutls_logger(int level, const char *message)
	{
		std::string msg(message);
		msg.resize(msg.size() - 1);

		TRACE("gnutls [%d] %s", level, msg.c_str());
	}

	// ssl.add(
	// 		'keyfile' => PATH,
	// 		'crtfile' => PATH,
	// 		'crlfile' => PATH);
	//
	void add_context(Flow::Value& result, const x0::Params& args)
	{
		std::auto_ptr<SslContext> cx(new SslContext());
		cx->setLogger(server().logger());
		std::string keyname;

		for (std::size_t i = 0, e = args.count(); i != e; ++i)
		{
			if (!args[i].isArray())
				continue;

			const Flow::Value *key = args[i].toArray();
			const Flow::Value *value = key + 1;

			if (!key->isString())
				continue;

			keyname = key->toString();

			if (!value->isString() && !value->isNumber() && !value->isBool())
				continue;

			std::string sval;
			if (keyname == "certfile") {
				if (!value->load(sval)) return;
				cx->certFile = sval;
			} else if (keyname == "keyfile") {
				if (!value->load(sval)) return;
				cx->keyFile = sval;
			} else if (keyname == "trustfile") {
				if (!value->load(sval)) return;
				cx->trustFile = sval;
			} else if (keyname == "priorities") {
				if (!value->load(sval)) return;
				cx->priorities = sval;
			} else {
				server().log(x0::Severity::error, "ssl: Unknown ssl.context key: '%s'\n", keyname.c_str());
				return;
			}
		}

		// context setup successful -> put into our ssl context set.
		contexts_.push_back(cx.release());
	}
	// }}}
};

X0_EXPORT_PLUGIN(ssl)
