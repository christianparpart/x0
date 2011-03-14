/* <x0/plugins/vhost.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2011 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: hostname resolver
 *
 * description:
 *     Maps the request hostname:port to a dedicated handler.
 *
 * setup API:
 *     void vhost.mapping(FQDN => handler_ref, ...);
 *
 * request processing API:
 *     handler vhost.map();
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/Types.h>

#include <unordered_map>

/**
 * \ingroup plugins
 * \brief virtual host mapping plugin
 */
class vhost_plugin :
	public x0::HttpPlugin
{
private:
	typedef std::map<std::string, Flow::Value::Function> NamedHostMap;

	NamedHostMap qualifiedHosts_;
	NamedHostMap unqualifiedHosts_;

public:
	vhost_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerSetupFunction<vhost_plugin, &vhost_plugin::addHost>("vhost.mapping", Flow::Value::VOID);
		registerHandler<vhost_plugin, &vhost_plugin::mapRequest>("vhost.map");
	}

	~vhost_plugin()
	{
	}

private:
	// vhost.add fqdn => proc, ...
	void addHost(Flow::Value& result, const x0::Params& args)
	{
		for (auto& arg: args)
			registerHost(arg);
	}

	void registerHost(const Flow::Value& arg)
	{
		if (arg.type() == Flow::Value::ARRAY) {
			const Flow::Value* args = arg.toArray();
			if (args[0].isVoid() || args[1].isVoid() || !args[2].isVoid())
				return;

			const Flow::Value& fqdn = args[0];
			const Flow::Value& proc = args[1];

			if (!fqdn.isString())
				return;

			if (!proc.isFunction())
				return;

			registerHost(fqdn.toString(), proc.toFunction());
		}
	}

	void registerHost(const char *fqdn, Flow::Value::Function handler)
	{
		if (strchr(fqdn, ':'))
			qualifiedHosts_[fqdn] = handler;
		else
			unqualifiedHosts_[fqdn] = handler;
	}

	bool mapRequest(x0::HttpRequest *r, const x0::Params& args)
	{
		auto i = qualifiedHosts_.find(r->hostid());
		if (i != qualifiedHosts_.end())
			return i->second(r);

		auto k = unqualifiedHosts_.find(r->hostname.str());
		if (k != unqualifiedHosts_.end())
			return k->second(r);

		return false;
	}
};

X0_EXPORT_PLUGIN(vhost)
