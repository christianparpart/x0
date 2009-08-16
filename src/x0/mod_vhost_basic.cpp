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
	boost::signals::connection c;

	struct vhost_config
	{
		std::string docroot;
	};

	struct server_config
	{
		std::string default_host;
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
		auto hostnames = server_.config()["Hosts"].keys<std::string>();
		if (hostnames.empty())
			return;

		std::string default_bind("0::0");
		server_.config().load("BindAddress", default_bind);

		int default_port(80);
		server_.config().load("Listen", default_port);

		server_config& srvcfg = server_.create_context<server_config>(this);

		server_.config().load("DefaultHost", srvcfg.default_host);
		if (srvcfg.default_host.find(":") == std::string::npos && default_port != 80)
		{
			srvcfg.default_host = srvcfg.default_host + ":" + boost::lexical_cast<std::string>(default_port);
		}

		for (auto i = hostnames.begin(), e = hostnames.end(); i != e; ++i)
		{
			std::string hostname(*i);

			auto aliases = server_.config()["Hosts"][hostname]["ServerAliases"].as<std::vector<std::string>>();

			std::string docroot = server_.config()["Hosts"][hostname]["DocumentRoot"].as<std::string>();

			std::string bind = server_.config()["Hosts"][hostname]["BindAddress"].as<std::string>();
			if (bind.empty())
				bind = default_bind;

			int port = extract_port(hostname);
			if (!port)
			{
				port = default_port;

				if (port != 80)
				{
					hostname = hostname + ":" + boost::lexical_cast<std::string>(port);
				}
			}

//			server_.log(x0::severity::debug, "port=%d: hostname[%s]: docroot[%s], alias-count=%d",
//				port, hostname.c_str(), docroot.c_str(), aliases.size());

			vhost_config& cfg = server_.create_context<vhost_config>(this, hostname);
			cfg.docroot = docroot;

			for (auto k = aliases.begin(), m = aliases.end(); k != m; ++k)
			{
				std::string name(*k);
				if (name.find(":") == std::string::npos && port != 80)
				{
					name = name + ":" + boost::lexical_cast<std::string>(port);
				}

				auto f = srvcfg.mappings.find(name);
				if (f == srvcfg.mappings.end())
				{
					srvcfg.mappings[name] = &cfg;
				}
				else
				{
					server_.log(x0::severity::warn, "Server alias '%s' already in use.", name.c_str());
				}
			}

			server_.setup_listener(port, bind);
		}
	}

private:
	int extract_port(const std::string& hostname)
	{
		static std::string delim(":");
		std::size_t n = hostname.find(delim);

		if (n == std::string::npos)
			return 0;

		return boost::lexical_cast<int>(hostname.substr(n + 1));
	}

	void resolve_document_root(x0::request& in)
	{
		try
		{
			static std::string hostkey("Host");
			std::string name(in.header(hostkey));

			server_config& srvcfg = server_.context<server_config>(this);
			auto i = srvcfg.mappings.find(name);
			if (i != srvcfg.mappings.end())
			{
				in.document_root = i->second->docroot;
			}
			else
			{
				in.document_root = server_.context<vhost_config>(this, name).docroot;
			}
		}
		catch (const x0::host_not_found&)
		{
			try
			{
				// resolve to default host's document root
				std::string default_host(server_.context<server_config>(this).default_host);
				in.document_root = server_.context<vhost_config>(this, default_host).docroot;
				// XXX we could instead auto-redirect them.
			}
			catch (...) {}
		}
	}
};

X0_EXPORT_PLUGIN(vhost_basic);
