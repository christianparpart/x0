/* <x0/plugins/proxy/ProxyContext.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "ProxyContext.h"
#include "ProxyConnection.h"

#include <ev++.h>

#if 1
#	define TRACE(msg...) /*!*/
#else
#	define TRACE(msg...) DEBUG("ProxyContext: " msg)
#endif

ProxyContext::ProxyContext(struct ev_loop *lp) :
	loop(lp),
	enabled(true),
	buffer_size(0),
	connect_timeout(8),
	read_timeout(0),
	write_timeout(8),
	keepalive(/* 10 */ 0),
	ignore_client_abort(false),
	origins(),
	hot_spares(),
	allowed_methods(),
	origins_ptr(0)
{
	TRACE("ProxyContext(%p) create", this);

	allowed_methods.push_back("GET");
	allowed_methods.push_back("HEAD");
	allowed_methods.push_back("POST");
}

ProxyContext::~ProxyContext()
{
	TRACE("ProxyContext(%p) destroy", this);
}

bool ProxyContext::method_allowed(const x0::BufferRef& method) const
{
	if (x0::equals(method, "GET"))
		return true;

	if (x0::equals(method, "HEAD"))
		return true;

	if (x0::equals(method, "POST"))
		return true;

	for (std::vector<std::string>::const_iterator i = allowed_methods.begin(), e = allowed_methods.end(); i != e; ++i)
		if (x0::equals(method, *i))
			return true;

	return false;
}

ProxyConnection *ProxyContext::acquire()
{
	ProxyConnection *pc;
	if (!idle_.empty())
	{
		pc = idle_.front();
		idle_.pop_front();
		TRACE("connection acquire.idle(%p)", pc);
	}
	else
	{
		pc = new ProxyConnection(this);
		TRACE("connection acquire.new(%p)", pc);

		pc->connect(origins[origins_ptr]);

		if (++origins_ptr >= origins.size())
			origins_ptr = 0;
	}

	return pc;
}

void ProxyContext::release(ProxyConnection *pc)
{
	TRACE("connection release(%p)", pc);
	idle_.push_back(pc);
}
