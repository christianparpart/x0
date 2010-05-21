/* <x0/mod_vhost_basic.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/plugin.hpp>
#include <x0/http/server.hpp>
#include <x0/http/request.hpp>
#include <x0/http/response.hpp>
#include <x0/http/listener.hpp>

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
	struct vhost_config
	{
		std::string name;
		std::string document_root;
		std::string bindaddress;
	};

	x0::server::request_parse_hook::connection c;
	std::map<std::string, vhost_config *> mappings; // [hostname:port] -> vhost_config
	std::map<int, vhost_config *> default_hosts; // [port] -> vhost_config

public:
	vhost_basic_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		using namespace std::placeholders;

		c = server_.resolve_document_root.connect(std::bind(&vhost_basic_plugin::resolve_document_root, this, _1));

		server_.register_cvar_host("DocumentRoot", std::bind(&vhost_basic_plugin::setup_docroot, this, _1, _2), 0);
		server_.register_cvar_host("Default", std::bind(&vhost_basic_plugin::setup_default, this, _1, _2), 1);
		server_.register_cvar_host("BindAddress", std::bind(&vhost_basic_plugin::setup_bindaddress, this, _1, _2), 1);
		server_.register_cvar_host("Aliases", std::bind(&vhost_basic_plugin::setup_aliases, this, _1, _2), 1);
	}

	~vhost_basic_plugin()
	{
		server_.resolve_document_root.disconnect(c);
	}

	void setup_docroot(const x0::settings_value& cvar, const std::string& hostid)
	{
		std::string document_root = cvar.as<std::string>();

		if (document_root.empty())
		{
			server_.log(x0::severity::error,
				"vhost_basic[%s]: document root must not be empty.",
				hostid.c_str());
		}

		if (document_root[0] != '/')
		{
			log(x0::severity::warn,
				"vhost_basic[%s]: document root should be an absolute path: '%s'",
				hostid.c_str(), document_root.c_str());
		}

		vhost_config *cfg = server_.create_context<vhost_config>(this, hostid);
		cfg->name = hostid;
		cfg->document_root = document_root;

		// register primary [hostname:port]
		if (!register_vhost(hostid, cfg))
		{
			server_.log(x0::severity::error, "Server name '%s' already in use.", hostid.c_str());
		}
	}

	void setup_bindaddress(const x0::settings_value& cvar, const std::string& hostid)
	{
		int port = x0::extract_port_from_hostid(hostid);
		std::string bind = cvar.as<std::string>();
		vhost_config *cfg = server_.context<vhost_config>(this, hostid);
		cfg->bindaddress = bind;

		server_.setup_listener(port, bind);
	}

	void setup_aliases(const x0::settings_value& cvar, const std::string& hostid)
	{
		auto aliases = cvar.as<std::vector<std::string>>();
		int port = x0::extract_port_from_hostid(hostid);
		vhost_config *cfg = server_.context<vhost_config>(this, hostid);

		for (auto k = aliases.begin(), m = aliases.end(); k != m; ++k)
		{
			std::string hid(x0::make_hostid(*k, port));

			if (!register_vhost(hid, cfg))
			{
				server_.log(x0::severity::error, "Server alias '%s' already in use.", hid.c_str());
			}

			server_.link_context(hostid, hid);

#if !defined(NDEBUG)
			debug(1, "Server alias '%s' (for bind '%s' on port %d) added.", hid.c_str(), cfg->bindaddress.c_str(), port);
#endif
		}
	}

	void setup_default(const x0::settings_value& cvar, const std::string& hostid)
	{
		bool is_default = cvar.as<bool>();
		int port = x0::extract_port_from_hostid(hostid);
		vhost_config *cfg = server_.context<vhost_config>(this, hostid);

		if (is_default)
		{
			if (vhost_config *vhost = get_default_vhost(port))
			{
				server_.log(x0::severity::error,
						"Cannot declare multiple virtual hosts as default "
						"with same port (%d). Conflicting hostnames: %s, %s.",
						port, vhost->name.c_str(), hostid.c_str());
			}

			set_default_vhost(port, cfg);
		}
	}

	/** maps domain-name to given vhost config.
	 * \retval true success
	 * \retval false failed, there is already a vhost assigned to this name.
	 */
	bool register_vhost(const std::string& name, vhost_config *cfg)
	{
		//debug(1, "Registering vhost: %s [%s]\n", name.c_str(), cfg->name.c_str());
		if (mappings.find(name) == mappings.end())
		{
			mappings.insert({{name, cfg}});
			return true;
		}
		return false;
	}

	void set_default_vhost(int port, vhost_config *cfg)
	{
#if !defined(NDEBUG)
		//debug(1, "set default host '%s'", cfg->name.c_str());
#endif
		default_hosts[port] = cfg;
	}

	vhost_config *get_default_vhost(int port)
	{
		auto i = default_hosts.find(port);

		if (i != default_hosts.end())
			return i->second;

		return NULL;
	}

	vhost_config *get_vhost(const std::string& name)
	{
		auto vh = mappings.find(name);
		if (vh != mappings.end())
			return vh->second;

		return NULL;
	}

	virtual void configure()
	{
		post_config();
	}

	void post_config()
	{
		// verify, that we have at least one vhost defined:
		// TODO

		// verify, that every listener has a default  vhost:
		for (auto i = server_.listeners().begin(), e = server_.listeners().end(); i != e; ++i)
		{
			if (!get_default_vhost((*i)->port()))
			{
				log(x0::severity::warn,
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
				debug(1, "no vhost config found for [%s]", hostid.c_str());
#endif
				return;
			}
			in->set_hostid(vhost->name);
		}

		in->document_root = vhost->document_root;

		//debug(1, "resolved [%s] to document_root [%s]", hostid.c_str(), in->document_root.c_str());
	}
};

X0_EXPORT_PLUGIN(vhost_basic);
