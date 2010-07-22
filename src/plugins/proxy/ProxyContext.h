/* <x0/plugins/proxy/ProxyContext.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_proxy_ProxyContext_h
#define sw_x0_proxy_ProxyContext_h (1)

#include "ProxyOrigin.h"

#include <x0/Scope.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/strutils.h>
#include <x0/Defines.h>
#include <x0/Types.h>

#include <string>
#include <vector>
#include <deque>

#include <ev++.h>

class ProxyConnection;

/** holds a complete proxy configuration for a specific entry point.
 */
class ProxyContext :
	public x0::ScopeValue
{
public:
	struct ev_loop *loop;

public:
	// public configuration properties
	bool enabled;
	std::size_t buffer_size;
	std::size_t connect_timeout;
	std::size_t read_timeout;
	std::size_t write_timeout;
	std::size_t keepalive;
	bool ignore_client_abort;
	std::vector<std::string> origins;
	std::vector<std::string> hot_spares;
	std::vector<std::string> allowed_methods;
	std::vector<ProxyOrigin> origins_;
	std::vector<std::string> ignores;

public:
	explicit ProxyContext(struct ev_loop *lp = 0);
	~ProxyContext();

	bool method_allowed(const x0::BufferRef& method) const;

	ProxyConnection *acquire();
	void release(ProxyConnection *px);

	virtual void merge(const x0::ScopeValue *from);

private:
	std::size_t origins_ptr;
	std::deque<ProxyConnection *> idle_;
};

#endif
