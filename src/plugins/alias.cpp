/* <x0/mod_alias.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>

/**
 * \ingroup plugins
 * \brief implements alias maps, mapping request paths to custom local paths (overriding resolved document_root concatation)
 */
class alias_plugin :
	public x0::HttpPlugin
{
private:
	x0::HttpServer::RequestHook::Connection c;

	typedef std::map<std::string, std::string> aliasmap_type;

	int alias_count_;

	struct context : public x0::ScopeValue
	{
		aliasmap_type aliases;

		virtual void merge(const x0::ScopeValue *scope)
		{
			if (auto cx = dynamic_cast<const context *>(scope))
			{
				aliases.insert(cx->aliases.begin(), cx->aliases.end());
			}
		}
	};

public:
	alias_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		alias_count_(0)
	{
		using namespace std::placeholders;
		c = srv.onResolveEntity.connect<alias_plugin, &alias_plugin::resolveEntity>(this);

		declareCVar("Aliases", x0::HttpContext::server | x0::HttpContext::host, &alias_plugin::setup);
	}

	~alias_plugin()
	{
		server_.onResolveEntity.disconnect(c);
		//server_.unlink_userdata(this);
	}

	virtual void post_config()
	{
		if (!alias_count_)
		{
			// Fine, you want me resident. But you did not make use of me!
			// So I'll disconnect myself from your service. See if I care!
			server_.onResolveEntity.disconnect(c);
		}
	}

private:
	std::error_code setup(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		std::error_code ec = cvar.load(s.acquire<context>(this)->aliases);

		if (ec)
			return ec;

		++alias_count_;

		return std::error_code();
	}

	inline aliasmap_type *get_aliases(x0::HttpRequest *in)
	{
		if (auto ctx = server_.resolveHost(in->hostid())->get<context>(this))
			return &ctx->aliases;

		return 0;
	}

	void resolveEntity(x0::HttpRequest *in)
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
