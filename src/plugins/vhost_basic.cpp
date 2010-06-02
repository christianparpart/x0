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
 *         BindAddress = '127.0.0.1';
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
	struct vhost_config : public x0::scope_value
	{
		std::string name;
		std::string document_root;
		std::string bind_address;

		virtual void merge(const x0::scope_value *value)
		{
			if (auto cx = dynamic_cast<const vhost_config *>(value))
			{
				if (name.empty())
					name = cx->name;

				if (document_root.empty())
					document_root = cx->document_root;

				if (bind_address.empty())
					bind_address = cx->bind_address;
			}
		}
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

		register_cvar("DocumentRoot", x0::context::vhost, &vhost_basic_plugin::setup_docroot);
		register_cvar("Default", x0::context::vhost, &vhost_basic_plugin::setup_default);
		register_cvar("BindAddress", x0::context::vhost, &vhost_basic_plugin::setup_bindaddress);
		register_cvar("ServerAliases", x0::context::vhost, &vhost_basic_plugin::setup_aliases);
	}

	~vhost_basic_plugin()
	{
		server_.resolve_document_root.disconnect(c);
	}

	bool setup_docroot(const x0::settings_value& cvar, x0::scope& s)
	{
		std::string document_root = cvar.as<std::string>();

		if (document_root.empty())
		{
			server_.log(x0::severity::error,
				"vhost_basic[%s]: document root must not be empty.",
				s.id().c_str());
			return false;
		}

		if (document_root[0] != '/')
		{
			log(x0::severity::warn,
				"vhost_basic[%s]: document root should be an absolute path: '%s'",
				s.id().c_str(), document_root.c_str());
		}

		vhost_config *cfg = s.acquire<vhost_config>(this);
		cfg->name = s.id();
		cfg->document_root = document_root;

		// register primary [hostname:port]
		if (!register_vhost(s.id(), cfg))
		{
			server_.log(x0::severity::error, "Server name '%s' already in use.", s.id().c_str());
			return false;
		}
		return true;
	}

	bool setup_bindaddress(const x0::settings_value& cvar, x0::scope& s)
	{
		int port = x0::extract_port_from_hostid(s.id());
		std::string bind = cvar.as<std::string>();
		vhost_config *cfg = s.acquire<vhost_config>(this);
		cfg->bind_address = bind;

		server_.setup_listener(port, bind);
		return true;
	}

	bool setup_aliases(const x0::settings_value& cvar, x0::scope& s)
	{
		std::vector<std::string> aliases;
		if (!cvar.load(aliases))
			return false;

		int port = x0::extract_port_from_hostid(s.id());
		vhost_config *cfg = s.acquire<vhost_config>(this);

		for (auto k = aliases.begin(), m = aliases.end(); k != m; ++k)
		{
			std::string alias_id(x0::make_hostid(*k, port));

			if (!register_vhost(alias_id, cfg))
			{
				server_.log(x0::severity::error, "Server alias '%s' already in use.", alias_id.c_str());
				return false;
			}

			server_.link_vhost(s.id(), alias_id);

#if !defined(NDEBUG)
			debug(1, "Server alias '%s' (for bind '%s' on port %d) added.", alias_id.c_str(), cfg->bind_address.c_str(), port);
#endif
		}
		return true;
	}

	bool setup_default(const x0::settings_value& cvar, x0::scope& s)
	{
		bool is_default;
		if (!cvar.load(is_default))
			return false;

		int port = x0::extract_port_from_hostid(s.id());
		vhost_config *cfg = s.acquire<vhost_config>(this);

		if (is_default)
		{
			if (vhost_config *vhost = get_default_vhost(port))
			{
				server_.log(x0::severity::error,
						"Cannot declare multiple virtual hosts as default "
						"with same port (%d). Conflicting hostnames: %s, %s.",
						port, vhost->name.c_str(), s.id().c_str());

				return false;
			}

			set_default_vhost(port, cfg);
		}
		return true;
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

	virtual void post_config()
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
	}

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
				debug(1, "no vhost config found for [%s]", in->hostid().c_str());
#endif
				return;
			}
			in->set_hostid(vhost->name);
		}

		in->document_root = vhost->document_root;

		debug(1, "resolved [%s] to document_root [%s]", hostid.c_str(), in->document_root.c_str());
	}
};

X0_EXPORT_PLUGIN(vhost_basic);
