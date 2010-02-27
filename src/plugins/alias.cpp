/* <x0/mod_alias.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

	struct context
	{
		aliasmap_type aliases;
	};

public:
	alias_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		c = server_.resolve_entity.connect(boost::bind(&alias_plugin::resolve_entity, this, _1));
	}

	~alias_plugin()
	{
		server_.resolve_entity.disconnect(c);
		server_.free_context<context>(this);
	}

	virtual void configure()
	{
		auto hosts = server_.config()["Hosts"].keys<std::string>();
		for (auto i = hosts.begin(), e = hosts.end(); i != e; ++i)
		{
			std::map<std::string, std::string> aliases;

			if (server_.config()["Hosts"][*i]["Aliases"].load(aliases)
			 || server_.config()["Aliases"].load(aliases))
			{
				server_.create_context<context>(this, *i).aliases = aliases;
			}
		}
	}

private:
	inline aliasmap_type *get_aliases(x0::request *in)
	{
		try
		{
			return &server_.context<context>(this, in->hostid()).aliases;
		}
		catch (...)
		{
			return 0;
		}
	}

	void resolve_entity(x0::request *in)
	{
		using boost::algorithm::starts_with;

		if (auto aliases = get_aliases(in))
		{
			for (auto i = aliases->begin(), e = aliases->end(); i != e; ++i)
			{
				if (starts_with(in->path, i->first))
				{
					in->fileinfo = server_.fileinfo(i->second + in->path.substr(i->first.size()));
				}
			}
		}
	}
};

X0_EXPORT_PLUGIN(alias);
