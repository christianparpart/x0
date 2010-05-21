/* <x0/mod_alias.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/plugin.hpp>
#include <x0/http/server.hpp>
#include <x0/http/request.hpp>

/**
 * \ingroup plugins
 * \brief implements alias maps, mapping request paths to custom local paths (overriding resolved document_root concatation)
 */
class alias_plugin :
	public x0::plugin
{
private:
	x0::server::request_parse_hook::connection c;

	typedef std::map<std::string, std::string> aliasmap_type;

	aliasmap_type global_aliases;
	std::map<std::string, aliasmap_type> host_aliases;

	struct context
	{
		aliasmap_type aliases;
	};

public:
	alias_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		using namespace std::placeholders;
		c = srv.resolve_entity.connect(std::bind(&alias_plugin::resolve_entity, this, _1));

		srv.register_cvar_host("Aliases", std::bind(&alias_plugin::setup_per_host, this, _1, _2));
		srv.register_cvar_server("Aliases", std::bind(&alias_plugin::setup_per_srv, this, _1));
	}

	~alias_plugin()
	{
		server_.resolve_entity.disconnect(c);
		server_.free_context<context>(this);
	}

	virtual void post_config()
	{
		if (global_aliases.empty() && host_aliases.empty())
		{
			// Fine, you want me resident. But you did not make use of me!
			// So I'll disconnect myself from your service. See if I care!
			server_.resolve_entity.disconnect(c);
		}
	}

private:
	void setup_per_host(const x0::settings_value& cvar, const std::string& hostid)
	{
		cvar.load(host_aliases[hostid]);
	}

	void setup_per_srv(const x0::settings_value& cvar)
	{
		cvar.load(global_aliases);
	}

	inline aliasmap_type *get_aliases(x0::request *in)
	{
		auto i = host_aliases.find(in->hostid());
		if (i != host_aliases.end())
			return &i->second;

		if (global_aliases.empty())
			return &global_aliases;

		return 0;
	}

	void resolve_entity(x0::request *in)
	{
		if (auto aliases = get_aliases(in))
		{
			for (auto i = aliases->begin(), e = aliases->end(); i != e; ++i)
			{
				if (in->path.begins(i->first))
				{
					in->fileinfo = server_.fileinfo(i->second + in->path.substr(i->first.size()));
					break;
				}
			}
		}
	}
};

X0_EXPORT_PLUGIN(alias);
