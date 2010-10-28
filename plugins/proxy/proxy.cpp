/* <x0/plugins/proxy/proxy.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "ProxyConnection.h"
#include "ProxyOrigin.h"

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
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
 * handler setup {
 * }
 *
 * handler main {
 *     proxy.reverse 'http://127.0.0.1:3000';
 * }
 *
 * -- possible tweaks:
 *  - bufsize
 *  - timeout.connect
 *  - timeout.write
 *  - timeout.read
 *  - ignore_clientabort
 * };
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
		registerHandler<proxy_plugin, &proxy_plugin::proxy_reverse>("proxy.reverse");
	}

	~proxy_plugin()
	{
	}

private:
	bool proxy_reverse(x0::HttpRequest *r, const x0::Params& args)
	{
		// TODO: reuse already spawned proxy connections instead of recreating each time.
		ProxyConnection *pc = new ProxyConnection(r->connection.worker().loop());
		if (!pc)
			return false;

		pc->connect(args[0].toString());

		pc->start(std::bind(&x0::HttpRequest::finish, r), r);

		return true;
	}
};

X0_EXPORT_PLUGIN(proxy)
