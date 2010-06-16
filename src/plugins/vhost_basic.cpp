/* <x0/mod_vhost_basic.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpListener.h>

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
	public x0::HttpPlugin
{
private:
	struct vhost_config : public x0::ScopeValue
	{
		std::string name;
		std::string document_root;
		std::string bind_address;

		virtual void merge(const x0::ScopeValue *value)
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

	x0::HttpServer::RequestHook::Connection c;
	std::map<std::string, vhost_config *> mappings; // [hostname:port] -> vhost_config
	std::map<int, vhost_config *> default_hosts; // [port] -> vhost_config

public:
	vhost_basic_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		using namespace std::placeholders;

		c = server_.onResolveDocumentRoot.connect<vhost_basic_plugin, &vhost_basic_plugin::resolveDocumentRoot>(this);

		declareCVar("DocumentRoot", x0::HttpContext::host, &vhost_basic_plugin::setup_docroot);
		declareCVar("Default", x0::HttpContext::host, &vhost_basic_plugin::setup_default);
		declareCVar("BindAddress", x0::HttpContext::host, &vhost_basic_plugin::setup_bindaddress);
		declareCVar("ServerAliases", x0::HttpContext::host, &vhost_basic_plugin::setup_aliases);
	}

	~vhost_basic_plugin()
	{
		server_.onResolveDocumentRoot.disconnect(c);
	}

	std::error_code setup_docroot(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		std::string document_root = cvar.as<std::string>();

		if (document_root.empty())
		{
			server_.log(x0::Severity::error,
				"vhost_basic[%s]: document root must not be empty.",
				s.id().c_str());
			return std::make_error_code(std::errc::invalid_argument);
		}

		if (document_root[0] != '/')
		{
			log(x0::Severity::warn,
				"vhost_basic[%s]: document root should be an absolute path: '%s'",
				s.id().c_str(), document_root.c_str());
		}

		vhost_config *cfg = s.acquire<vhost_config>(this);
		cfg->name = s.id();
		cfg->document_root = document_root;

		// register primary [hostname:port]
		if (!register_host(s.id(), cfg))
		{
			server_.log(x0::Severity::error, "Server name '%s' already in use.", s.id().c_str());
			return std::make_error_code(std::errc::invalid_argument);
		}

		return std::error_code();
	}

	std::error_code setup_bindaddress(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		std::string bindAddress;
		std::error_code ec = cvar.load(bindAddress);
		if (ec)
			return ec;

		int port = x0::extract_port_from_hostid(s.id());

		vhost_config *cfg = s.acquire<vhost_config>(this);
		cfg->bind_address = bindAddress;

		server_.setupListener(port, bindAddress);

		return std::error_code();
	}

	std::error_code setup_aliases(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		std::vector<std::string> aliases;
		std::error_code ec = cvar.load(aliases);
		if (ec)
			return ec;

		int port = x0::extract_port_from_hostid(s.id());
		vhost_config *cfg = s.acquire<vhost_config>(this);

		for (auto k = aliases.begin(), m = aliases.end(); k != m; ++k)
		{
			std::string alias_id(x0::make_hostid(*k, port));

			if (!register_host(alias_id, cfg))
			{
				server_.log(x0::Severity::error, "Server alias '%s' already in use.", alias_id.c_str());
				return std::make_error_code(std::errc::invalid_argument);
			}

			server_.linkHost(s.id(), alias_id);

#if !defined(NDEBUG)
			debug(1, "Server alias '%s' (for bind '%s' on port %d) added.", alias_id.c_str(), cfg->bind_address.c_str(), port);
#endif
		}
		return std::error_code();
	}

	std::error_code setup_default(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		bool is_default;
		std::error_code ec = cvar.load(is_default);
		if (ec) return ec;

		int port = x0::extract_port_from_hostid(s.id());
		vhost_config *cfg = s.acquire<vhost_config>(this);

		if (is_default)
		{
			if (vhost_config *vhost = get_default_host(port))
			{
				server_.log(x0::Severity::error,
						"Cannot declare multiple virtual hosts as default "
						"with same port (%d). Conflicting hostnames: %s, %s.",
						port, vhost->name.c_str(), s.id().c_str());

				return std::make_error_code(std::errc::invalid_argument);
			}

			set_default_host(port, cfg);
		}
		return std::error_code();
	}

	/** maps domain-name to given vhost config.
	 * \retval true success
	 * \retval false failed, there is already a vhost assigned to this name.
	 */
	bool register_host(const std::string& name, vhost_config *cfg)
	{
		//debug(1, "Registering vhost: %s [%s]\n", name.c_str(), cfg->name.c_str());
		if (mappings.find(name) == mappings.end())
		{
			mappings.insert({{name, cfg}});
			return true;
		}
		return false;
	}

	void set_default_host(int port, vhost_config *cfg)
	{
#if !defined(NDEBUG)
		//debug(1, "set default host '%s'", cfg->name.c_str());
#endif
		default_hosts[port] = cfg;
	}

	vhost_config *get_default_host(int port)
	{
		auto i = default_hosts.find(port);

		if (i != default_hosts.end())
			return i->second;

		return NULL;
	}

	vhost_config *get_host(const std::string& name)
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
			if (!get_default_host((*i)->port()))
			{
				log(x0::Severity::warn,
					"No default host defined for listener at port %d.",
					(*i)->port());
			}
		}
	}

private:
	void resolveDocumentRoot(x0::HttpRequest *in)
	{
		std::string hostid(in->hostid());

		vhost_config *vhost = get_host(hostid);
		if (!vhost)
		{
			vhost = get_default_host(in->connection.local_port());
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
