/* <x0/mod_vhost_basic.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/listener.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/signal.hpp>
#include <boost/bind.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * \ingroup plugins
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
 *
 *         Secure = true;
 *         CertFile = 'cert.pem';
 *         KeyFile = 'key.pem';
 *         TrustFile = 'ca.pem';
 *         CrlFile = 'crl.pem';
 *     };
 * };
 * </pre>
 */
class vhost_basic_plugin :
	public x0::plugin
{
private:
	x0::server::request_parse_hook::connection c;

	struct vhost_config
	{
		std::string document_root;
	};

	struct server_config
	{
		std::string default_hostid;
		std::map<std::string, vhost_config *> mappings; // [hostname:port] -> vhost_config
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
			return; // no hosts configured

		std::string default_bind("0::0");
		server_.config().load("BindAddress", default_bind);

		server_config& srvcfg = server_.create_context<server_config>(this);

		server_.config().load("DefaultHost", srvcfg.default_hostid);

		for (auto i = hosts.begin(), e = hosts.end(); i != e; ++i)
		{
			std::string hostid = x0::make_hostid(*i);
			int port = x0::extract_port_from_hostid(hostid);

			auto aliases = server_.config()["Hosts"][hostid]["ServerAliases"].as<std::vector<std::string>>();
			std::string document_root = server_.config()["Hosts"][hostid]["DocumentRoot"].as<std::string>();
			std::string bind = server_.config()["Hosts"][hostid]["BindAddress"].get<std::string>(default_bind);

			//server_.log(x0::severity::debug, "port=%d: hostid[%s]: document_root[%s], alias-count=%d",
			//	port, hostid.c_str(), document_root.c_str(), aliases.size());

			vhost_config& cfg = server_.create_context<vhost_config>(this, hostid);
			cfg.document_root = document_root;

#if defined(WITH_SSL)
			x0::listener *listener = server_.setup_listener(port, bind);

			if (server_.config()["Hosts"][hostid]["Secure"].get<bool>(false))
			{
				listener->trust_file(server_.config()["Hosts"][hostid]["TrustFile"].get<std::string>());
				listener->crl_file(server_.config()["Hosts"][hostid]["CrlFile"].get<std::string>());
				listener->key_file(server_.config()["Hosts"][hostid]["KeyFile"].get<std::string>());
				listener->cert_file(server_.config()["Hosts"][hostid]["CertFile"].get<std::string>());

				listener->secure(true);
			}
#else
			server_.setup_listener(port, bind);
#endif

			// insert primary [hostname:port]
			srvcfg.mappings.insert({ {hostid, &cfg} });

			// register vhost aliases
			for (auto k = aliases.begin(), m = aliases.end(); k != m; ++k)
			{
				std::string hid(x0::make_hostid(*k, port));

				auto f = srvcfg.mappings.find(hid);
				if (f == srvcfg.mappings.end())
				{
					srvcfg.mappings[hid] = &cfg;
					server_.link_context(hostid, hid);

					//server_.log(x0::severity::debug, "Server alias '%s' (for bind '%s' on port %d) added.", hid.c_str(), bind.c_str(), port);
				}
				else
				{
					char msg[1024];
					snprintf(msg, sizeof(msg), "Server alias '%s' already in use.", hid.c_str());
					throw std::runtime_error(msg);
				}
			}
		}

		// make sure the default-host has been defined (if specified)
		if (!srvcfg.default_hostid.empty())
		{
			auto vhost = srvcfg.mappings.find(srvcfg.default_hostid);

			if (vhost == srvcfg.mappings.end())
			{
				char msg[1024];
				snprintf(msg, sizeof(msg), "DefaultHost '%s' not defined.", srvcfg.default_hostid.c_str());
				throw std::runtime_error(msg);
			}
		}
	}

private:
	void resolve_document_root(x0::request *in)
	{
		try
		{
			std::string hostid(in->hostid());

			server_config& srvcfg = server_.context<server_config>(this);

			auto vhost = srvcfg.mappings.find(hostid);
			if (vhost == srvcfg.mappings.end())
			{
				if (srvcfg.default_hostid.empty())
					return;

				vhost = srvcfg.mappings.find(srvcfg.default_hostid);
			}

			in->document_root = vhost->second->document_root;

			//server_.log(x0::severity::debug, "vhost_basic: resolved [%s] to document_root [%s]",
			//	hostid.c_str(), in->document_root.c_str());
		}
		catch (const x0::host_not_found&)
		{
			try
			{
				// resolve to default host's document root
				std::string hostid(server_.context<server_config>(this).default_hostid);
				in->document_root = server_.context<vhost_config>(this, hostid).document_root;

				//server_.log(x0::severity::debug, "vhost_basic: resolved [%s] to document_root [%s] (via default vhost)",
				//	hostid.c_str(), in->document_root.c_str());
			}
			catch (...) {}
		}
	}
};

X0_EXPORT_PLUGIN(vhost_basic);
