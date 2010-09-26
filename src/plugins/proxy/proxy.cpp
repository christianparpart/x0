/* <x0/plugins/proxy/proxy.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "ProxyConnection.h"
#include "ProxyOrigin.h"
#include "ProxyContext.h"

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/strutils.h>
#include <x0/Url.h>
#include <x0/Types.h>

// CODE CLEANUPS:
// - TODO Create ProxyConnection at pre_process to connect to backend and pass
// 			possible request content already
// - TODO Implement proper error handling (origin connect or i/o errors,
// 			and client disconnect event).
// - TODO Should we use getaddrinfo() instead of inet_pton()?
// - TODO Implement proper timeout management for connect/write/read/keepalive
// 			timeouts.
// FEATURES:
// - TODO Implement proper node load balancing.
// - TODO Implement proper hot-spare fallback node activation, 

/* -- configuration proposal:
 *
 * ['YourDomain.com'] = {
 *     ProxyEnabled = true;
 *     ProxyBufferSize = 0;
 *     ProxyConnectTimeout = 5;
 *     ProxyIgnoreClientAbort = false;
 *     ProxyMode = "reverse";                 -- "reverse" | "forward" | and possibly others
 *     ProxyKeepAlive = 0;                    -- keep-alive seconds to origin servers
 *     ProxyMethods = { 'PROPFIND' };
 *     ProxyServers = {
 *         "http://pr1.backend/"
 *     };
 *     ProxyHotSpares = {
 *         "http://hs1.backend.net/"
 *     };
 * };
 *
 * Allowed Contexts: vhost, location
 *
 */

#if 1
#	define TRACE(msg...) /*!*/
#else
#	define TRACE(msg...) DEBUG("proxy: " msg)
#endif

/**
 * \ingroup plugins
 * \brief proxy content generator plugin
 */
class proxy_plugin :
	public x0::HttpPlugin
{
public:
	proxy_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		using namespace std::placeholders;

		declareCVar("ProxyEnable", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_enable);
		declareCVar("ProxyMode", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_mode);
		declareCVar("ProxyOrigins", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_origins);
		declareCVar("ProxyHotSpares", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_hotspares);
		declareCVar("ProxyMethods", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_methods);
		declareCVar("ProxyConnectTimeout", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_connect_timeout);
		declareCVar("ProxyReadTimeout", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_read_timeout);
		declareCVar("ProxyWriteTimeout", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_write_timeout);
		declareCVar("ProxyKeepAliveTimeout", x0::HttpContext::server | x0::HttpContext::host, &proxy_plugin::setup_proxy_keepalive_timeout);
	}

	~proxy_plugin()
	{
	}

private:
	std::error_code setup_proxy_enable(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(acquire_proxy(s)->enabled);
	}

	std::error_code setup_proxy_mode(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		// TODO reverse / forward / transparent (forward)
		return std::error_code();
	}

	std::error_code setup_proxy_origins(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		ProxyContext *px = acquire_proxy(s);
		cvar.load(px->origins);

		for (std::size_t i = 0, e = px->origins.size(); i != e; ++i)
		{
			std::string url = px->origins[i];
			std::string protocol, hostname;
			int port = 0;

			if (!x0::parseUrl(url, protocol, hostname, port))
			{
				TRACE("%s.", "Origin URL parse error");
				continue;
			}

			ProxyOrigin origin(hostname, port);
			if (origin.is_enabled())
				px->origins_.push_back(origin);
			else
				server_.log(x0::Severity::error, origin.error().c_str());
		}

		if (!px->origins_.empty())
			return std::error_code();

		//! \bug FIX server_.log(x0::Severity::warn, "No origin servers defined for proxy at virtual-host: %s.", hostid.c_str());
		//return ProxyError::EmptyOriginSet;
		return std::error_code();
	}

	std::error_code setup_proxy_hotspares(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		//ProxyContext *px = acquire_proxy(s);
		//cvar.load(px->hot_spares);
		return std::error_code();
	}

	std::error_code setup_proxy_methods(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(acquire_proxy(s)->allowed_methods);
	}

	std::error_code setup_proxy_connect_timeout(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(acquire_proxy(s)->connect_timeout);
	}

	std::error_code setup_proxy_read_timeout(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(acquire_proxy(s)->read_timeout);
	}

	std::error_code setup_proxy_write_timeout(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(acquire_proxy(s)->write_timeout);
	}

	std::error_code setup_proxy_keepalive_timeout(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(acquire_proxy(s)->keepalive);
	}

	ProxyContext *acquire_proxy(x0::Scope& s)
	{
		if (ProxyContext *px = s.get<ProxyContext>(this))
			return px;

		auto px = std::make_shared<ProxyContext>(server_.loop());
		s.set(this, px);

		return px.get();
	}

	ProxyContext *get_proxy(x0::HttpRequest *in)
	{
		return server_.resolveHost(in->hostid())->get<ProxyContext>(this);
	}

public:
	virtual bool post_config()
	{
		// TODO ensure, that every ProxyContext instance is properly equipped.
		return true;
	}

private:
	virtual bool handleRequest(x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		ProxyContext *px = get_proxy(in);
		if (!px)
			return false;

		if (!px->enabled)
			return false;

		if (!px->method_allowed(in->method))
		{
			out->status = x0::http_error::method_not_allowed;
			out->finish();
			return true;
		}

		ProxyConnection *connection = px->acquire();
		connection->start(std::bind(&x0::HttpResponse::finish, out), in, out);
		return true;
	}
};

X0_EXPORT_PLUGIN(proxy);
