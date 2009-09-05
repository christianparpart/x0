/* <x0/mod_vhost_basic.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/signal.hpp>
#include <boost/bind.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * \ingroup modules
 * \brief provides a basic config-file based virtual hosting facility.
 *
 * <pre>
 * -- example configuration
 *
 * BindAddress = '0::0';             -- default bind address
 * Listen = 80;                      -- default listening port
 * DefaultHost = 'www.example.com';  -- default vhost to choose when we receive an unknown Host-request-header.
 *
 * Hosts = {
 *     ['www.example.com'] = {
 *         ServerAliases = { 'www.example.net', 'example.com', 'example.net' };
 *         DocumentRoot = '/var/www/example.com/htdocs';
 *     };
 *     ['localhost:8080'] = {
 *         DocumentRoot = '/var/www/example.com/htdocs';
 *         BindAddress = 'localhost';
 *     };
 * };
 * </pre>
 */
class vhost_basic_plugin :
	public x0::plugin
{
private:
	x0::signal<void(x0::request&)>::connection c;

	struct vhost_config
	{
		std::string docroot;
	};

	struct server_config
	{
		std::string default_hostid;
		std::map<std::string, vhost_config *> mappings;
	};

public:
	vhost_basic_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		c = server_.resolve_document_root.connect(boost::bind(&vhost_basic_plugin::resolve_document_root, this, _1));
	}

	~vhost_basic_plugin()
	{
		server_.resolve_document_root.disconnect(c);
	}

	virtual void configure()
	{
		auto hosts = server_.config()["Hosts"].keys<std::string>();
		if (hosts.empty())
			return;

		std::string default_bind("0::0");
		server_.config().load("BindAddress", default_bind);

		server_config& srvcfg = server_.create_context<server_config>(this);

		server_.config().load("DefaultHost", srvcfg.default_hostid);

		for (auto i = hosts.begin(), e = hosts.end(); i != e; ++i)
		{
			std::string hostid = x0::make_hostid(*i);
			int port = x0::extract_port_from_hostid(hostid);

			auto aliases = server_.config()["Hosts"][hostid]["ServerAliases"].as<std::vector<std::string>>();
			std::string docroot = server_.config()["Hosts"][hostid]["DocumentRoot"].as<std::string>();
			std::string bind = server_.config()["Hosts"][hostid]["BindAddress"].get<std::string>(default_bind);

//			server_.log(x0::severity::debug, "port=%d: hostid[%s]: docroot[%s], alias-count=%d",
//				port, hostid.c_str(), docroot.c_str(), aliases.size());

			vhost_config& cfg = server_.create_context<vhost_config>(this, hostid);
			cfg.docroot = docroot;

			srvcfg.mappings.insert({ {hostid, &cfg} });
			for (auto k = aliases.begin(), m = aliases.end(); k != m; ++k)
			{
				std::string hid(x0::make_hostid(*k, port));

				auto f = srvcfg.mappings.find(hid);
				if (f == srvcfg.mappings.end())
				{
					srvcfg.mappings[hid] = &cfg;
					server_.link_context(hostid, hid);

					int aport = x0::extract_port_from_hostid(hid);
					server_.setup_listener(aport, bind);
					//server_.log(x0::severity::debug, "Server alias '%s' (for bind '%s' on port %d) added.", hid.c_str(), bind.c_str(), aport);
				}
				else
				{
					char msg[1024];
					snprintf(msg, sizeof(msg), "Server alias '%s' already in use.", hid.c_str());
					throw std::runtime_error(msg);
				}
			}

			server_.setup_listener(port, bind);
		}
	}

private:
	void resolve_document_root(x0::request& in)
	{
		try
		{
			static std::string hostkey("Host");
			std::string hostid(x0::make_hostid(in.header(hostkey), in.connection.socket().local_endpoint().port()));

			server_config& srvcfg = server_.context<server_config>(this);
			auto i = srvcfg.mappings.find(hostid);
			if (i != srvcfg.mappings.end())
			{
				in.document_root = i->second->docroot;
				//server_.log(x0::severity::debug, "vhost_basic[%s]: resolved to %s", hostid.c_str(), in.document_root.c_str());
			}
			else
			{
				in.document_root = server_.context<vhost_config>(this, hostid).docroot;
			}
		}
		catch (const x0::host_not_found&)
		{
			try
			{
				// resolve to default host's document root
				std::string default_hostid(server_.context<server_config>(this).default_hostid);
				in.document_root = server_.context<vhost_config>(this, default_hostid).docroot;
				// XXX we could instead auto-redirect them.
			}
			catch (...) {}
		}
	}
};

X0_EXPORT_PLUGIN(vhost_basic);
