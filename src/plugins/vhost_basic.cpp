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
		std::string name;
		std::string document_root;
	};

	struct server_config
	{
		std::map<std::string, vhost_config *> mappings; // [hostname:port] -> vhost_config
		std::map<int, vhost_config *> default_hosts; // [port] -> vhost_config
	};

public:
	vhost_basic_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		c = server_.resolve_document_root.connect(boost::bind(&vhost_basic_plugin::resolve_document_root, this, _1));
		server_.create_context<server_config>(this);
	}

	~vhost_basic_plugin()
	{
		server_.resolve_document_root.disconnect(c);
	}

	/** maps domain-name to given vhost config.
	 * \retval true success
	 * \retval false failed, there is already a vhost assigned to this name.
	 */
	bool register_vhost(const std::string& name, vhost_config *cfg)
	{
		//DEBUG("Registering vhost: %s [%s]\n", name.c_str(), cfg->name.c_str());
		server_config *srvcfg = server_.context<server_config>(this);

		if (srvcfg->mappings.find(name) == srvcfg->mappings.end())
		{
			srvcfg->mappings.insert({{name, cfg}});
			return true;
		}
		return false;
	}

	void set_default_vhost(int port, vhost_config *cfg)
	{
#if 0 // !defined(NDEBUG)
		server_.log(x0::severity::debug, "set default host '%s'", cfg->name.c_str());
#endif

		server_config *srvcfg = server_.context<server_config>(this);
		srvcfg->default_hosts[port] = cfg;
	}

	vhost_config *get_default_vhost(int port)
	{
		server_config *srvcfg = server_.context<server_config>(this);
		auto i = srvcfg->default_hosts.find(port);

		if (i != srvcfg->default_hosts.end())
			return i->second;

		return NULL;
	}

	vhost_config *get_vhost(const std::string& name)
	{
		server_config *srvcfg = server_.context<server_config>(this);

		auto vh = srvcfg->mappings.find(name);
		if (vh != srvcfg->mappings.end())
			return vh->second;

		return NULL;
	}

	virtual void configure() // {{{
	{
		auto hosts = server_.config()["Hosts"].keys<std::string>();
		if (hosts.empty())
		{
			server_.log(x0::severity::notice, "vhost_basic: no virtual hosts configured");
			return; // no hosts configured
		}

		std::string default_bind("0::0");
		server_.config().load("BindAddress", default_bind);

		for (auto i = hosts.begin(), e = hosts.end(); i != e; ++i) // {{{ register vhosts
		{
			std::string hostid = x0::make_hostid(*i);
			int port = x0::extract_port_from_hostid(hostid);

			auto aliases = server_.config()["Hosts"][hostid]["ServerAliases"].as<std::vector<std::string>>();
			std::string document_root = server_.config()["Hosts"][hostid]["DocumentRoot"].as<std::string>();
			std::string bind = server_.config()["Hosts"][hostid]["BindAddress"].get<std::string>(default_bind);
			bool is_default = server_.config()["Hosts"][hostid]["Default"].get<bool>(false);

#if 0 // !defined(NDEBUG)
			server_.log(x0::severity::debug, "port=%d: hostid[%s]: document_root[%s], alias-count=%d",
				port, hostid.c_str(), document_root.c_str(), aliases.size());
#endif

			if (document_root.empty())
			{
				char msg[256];
				std::snprintf(msg, sizeof(msg),
					"vhost_basic[%s]: document root must not be empty.",
					hostid.c_str());

				throw std::runtime_error(msg);
			}

			if (document_root[0] != '/')
			{
				server_.log(x0::severity::warn,
					"vhost_basic[%s]: document root should be an absolute path: '%s'",
					hostid.c_str(), document_root.c_str());
			}

			// validate is_default-flag
			if (is_default)
			{
				if (vhost_config *vhost = get_default_vhost(port))
				{
					char msg[256];

					std::snprintf(msg, sizeof(msg),
							"Cannot declare multiple virtual hosts as default "
							"with same port (%d). Conflicting hostnames: %s, %s.",
							port, vhost->name.c_str(), hostid.c_str());

					throw std::runtime_error(msg);
				}
			}

			vhost_config *cfg = server_.create_context<vhost_config>(this, hostid);

			cfg->name = hostid;
			cfg->document_root = document_root;

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

			// register primary [hostname:port]
			if (!register_vhost(hostid, cfg))
			{
				char msg[1024];
				snprintf(msg, sizeof(msg), "Server name '%s' already in use.", hostid.c_str());
				throw std::runtime_error(msg);
			}

			// register vhost aliases
			for (auto k = aliases.begin(), m = aliases.end(); k != m; ++k)
			{
				std::string hid(x0::make_hostid(*k, port));

				if (!register_vhost(hid, cfg))
				{
					char msg[1024];
					snprintf(msg, sizeof(msg), "Server alias '%s' already in use.", hid.c_str());
					throw std::runtime_error(msg);
				}

				server_.link_context(hostid, hid);

#if 0 // !defined(NDEBUG)
				server_.log(x0::severity::debug,
					"Server alias '%s' (for bind '%s' on port %d) added.",
					hid.c_str(), bind.c_str(), port);
#endif
			}

			if (is_default)
				set_default_vhost(port, cfg);
		} // }}}

		// sanitiy-check listeners: every listener must have a default 
		for (auto i = server_.listeners().begin(), e = server_.listeners().end(); i != e; ++i)
		{
			if (!get_default_vhost((*i)->port()))
			{
				server_.log(x0::severity::warn,
					"No default host defined for listener at port %d.",
					(*i)->port());
			}
		}
	} // }}}

private:
	void resolve_document_root(x0::request *in)
	{
		std::string hostid(in->hostid());

		vhost_config *vhost = get_vhost(hostid);
		if (!vhost)
		{
			vhost = get_default_vhost(in->connection.local_port());
			if (!vhost)
			{
#if !defined(NDEBUG)
				server_.log(x0::severity::debug,
						"vhost_basic: no vhost config found for [%s]",
						hostid.c_str());
#endif

				return;
			}
			in->set_hostid(vhost->name);
		}

		in->document_root = vhost->document_root;

		//DEBUG("vhost_basic: resolved [%s] to document_root [%s]", hostid.c_str(), in->document_root.c_str());
	}
};

X0_EXPORT_PLUGIN(vhost_basic);
