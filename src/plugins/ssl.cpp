/* <ssl.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpListener.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/SslContext.h>
#include <x0/SslDriver.h>
#include <x0/SslSocket.h>
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

#define TRACE(msg...) DEBUG("ssl: " msg)

using x0::SslSocket;
using x0::SslDriver;
using x0::SslContext;

/**
 * \ingroup plugins
 * \brief SSL plugin
 */
class ssl_plugin :
	public x0::HttpPlugin,
	public x0::SslContextSelector
{
public:
	ssl_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		auto cmask = x0::HttpContext::server | x0::HttpContext::host;

		declareCVar("SslLogLevel", x0::HttpContext::server, &ssl_plugin::setupLogLevel);
		declareCVar("SslEnabled", cmask, &ssl_plugin::setupEnabled);
		declareCVar("SslCertFile", cmask, &ssl_plugin::setupCertFile);
		declareCVar("SslKeyFile", cmask, &ssl_plugin::setupKeyFile);
		declareCVar("SslCrlFile", cmask, &ssl_plugin::setupCrlFile);
		declareCVar("SslTrustFile", cmask, &ssl_plugin::setupTrustFile);
		declareCVar("SslPriorities", cmask, &ssl_plugin::setupPriorities);

		gnutls_global_init();
	}

	~ssl_plugin()
	{
		gnutls_global_deinit();
	}

	std::vector<SslContext *> contexts_;

	virtual SslContext *select(const std::string& dnsName) const
	{
		for (auto i = contexts_.begin(), e = contexts_.end(); i != e; ++i)
		{
			SslContext *cx = *i;

			if (cx->commonName() == dnsName)
				return cx;
		}

		return NULL;
	}

	// {{{ post_config
	void post_config()
	{
		// iterate through all virtual hosts and install the SslDriver if SSL was configured on it.
		auto hostnames = server().hostnames();
		for (auto i = hostnames.begin(), e = hostnames.end(); i != e; ++i)
		{
			SslContext *cx = server().host(*i).get<SslContext>(this);
			x0::HttpListener *listener = server().listenerByHost(*i);

			// skip if no SSL was confgiured/enabled on this virtual host (or no TCP listener was found)
			if (!listener || !cx || !cx->enabled)
				continue;

			log(x0::Severity::debug, "Enable SSL on host: %s", i->c_str());

			contexts_.push_back(cx);

			SslDriver *driver = new SslDriver(server().loop(), this);
			listener->setSocketDriver(driver);
			cx->setDriver(driver);
			cx->post_config();
		}
	} // }}}

	// {{{ config
private:
	std::error_code setupLogLevel(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		TRACE("setupLogLevel(cvar, scope)");
		//int level = 0;
		//if (cvar.load(level))
		//	setLogLevel(level);
		setLogLevel(cvar.as<int>());

		return std::error_code();
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

	std::error_code setupEnabled(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<SslContext>(this)->enabled);
	}

	std::error_code setupCertFile(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<SslContext>(this)->certFile);
	}

	std::error_code setupKeyFile(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<SslContext>(this)->keyFile);
	}

	std::error_code setupCrlFile(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<SslContext>(this)->crlFile);
	}

	std::error_code setupTrustFile(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<SslContext>(this)->trustFile);
	}

	std::error_code setupPriorities(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<SslContext>(this)->priorities);
	}
	// }}}
};

X0_EXPORT_PLUGIN(ssl);
