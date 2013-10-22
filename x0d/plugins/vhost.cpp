/* <x0/plugins/vhost.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
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

#include <x0d/XzeroPlugin.h>
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
	public x0d::XzeroPlugin
{
private:
	typedef std::map<std::string, x0::FlowValue::Function> NamedHostMap;

	NamedHostMap qualifiedHosts_;
	NamedHostMap unqualifiedHosts_;

public:
	vhost_plugin(x0d::XzeroDaemon* d, const std::string& name) :
		x0d::XzeroPlugin(d, name)
	{
		registerSetupFunction<vhost_plugin, &vhost_plugin::addHost>("vhost.mapping", x0::FlowValue::VOID);
		registerHandler<vhost_plugin, &vhost_plugin::mapRequest>("vhost.map");
	}

	~vhost_plugin()
	{
	}

private:
	// vhost.add fqdn => proc, ...
	void addHost(const x0::FlowParams& args, x0::FlowValue& result)
	{
		for (auto& arg: args)
			registerHost(arg);
	}

	void registerHost(const x0::FlowValue& arg)
	{
		if (arg.type() == x0::FlowValue::ARRAY) {
			const x0::FlowArray& args = arg.toArray();
			if (args.size() != 2)
				return;

			const x0::FlowValue& fqdn = args[0];
			const x0::FlowValue& proc = args[1];

			if (!fqdn.isString())
				return;

			if (!proc.isFunction())
				return;

			registerHost(fqdn.toString(), proc.toFunction());
		}
	}

	void registerHost(const char *fqdn, x0::FlowValue::Function handler)
	{
		server().log(x0::Severity::debug, "vhost: registering virtual host %s", fqdn);

		if (strchr(fqdn, ':'))
			qualifiedHosts_[fqdn] = handler;
		else
			unqualifiedHosts_[fqdn] = handler;
	}

	bool mapRequest(x0::HttpRequest *r, const x0::FlowParams& args)
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
