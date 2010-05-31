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

	int alias_count_;

	struct context : public x0::scope_value
	{
		aliasmap_type aliases;

		virtual void merge(const x0::scope_value *scope)
		{
			if (auto cx = dynamic_cast<const context *>(scope))
			{
				aliases.insert(cx->aliases.begin(), cx->aliases.end());
			}
		}
	};

public:
	alias_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		alias_count_(0)
	{
		using namespace std::placeholders;
		c = srv.resolve_entity.connect(std::bind(&alias_plugin::resolve_entity, this, _1));

		srv.register_cvar("Aliases", x0::context::server | x0::context::vhost, std::bind(&alias_plugin::setup, this, _1, _2));
	}

	~alias_plugin()
	{
		server_.resolve_entity.disconnect(c);
		//server_.unlink_userdata(this);
	}

	virtual void post_config()
	{
		if (!alias_count_)
		{
			// Fine, you want me resident. But you did not make use of me!
			// So I'll disconnect myself from your service. See if I care!
			server_.resolve_entity.disconnect(c);
		}
	}

private:
	bool setup(const x0::settings_value& cvar, x0::scope& s)
	{
		if (!cvar.load(s.acquire<context>(this)->aliases))
			return false;

		++alias_count_;

		return true;
	}

	inline aliasmap_type *get_aliases(x0::request *in)
	{
		if (auto ctx = server_.vhost(in->hostid()).get<context>(this))
			return &ctx->aliases;

		return 0;
	}

	void resolve_entity(x0::request *in)
	{
		if (in->path.size() < 2)
			return;

		if (auto aliases = get_aliases(in))
		{
			for (auto i = aliases->begin(), e = aliases->end(); i != e; ++i)
			{
				if (in->path.begins(i->first))
				{
					in->fileinfo = server_.fileinfo(i->second + in->path.substr(i->first.size()));
					//debug(0, "resolve_entity: %s [%s]: %s", i->first.c_str(), in->path.str().c_str(), in->fileinfo->filename().c_str());
					break;
				}
			}
		}
	}
};

X0_EXPORT_PLUGIN(alias);
