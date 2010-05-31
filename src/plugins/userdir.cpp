/* <x0/mod_userdir.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/plugin.hpp>
#include <x0/http/server.hpp>
#include <x0/http/request.hpp>
#include <x0/http/response.hpp>
#include <x0/http/header.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <pwd.h>

/**
 * \ingroup plugins
 * \brief implements automatic index file resolving, if mapped request path is a path.
 */
class userdir_plugin :
	public x0::plugin
{
private:
	x0::server::request_parse_hook::connection c;

	struct context : public x0::scope_value
	{
		std::string dirname_;

		virtual void merge(const x0::scope_value *value)
		{
			if (auto cx = dynamic_cast<const context *>(value))
			{
				if (dirname_.empty())
					dirname_ = cx->dirname_;
			}
		}
	};

public:
	userdir_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		using namespace std::placeholders;
		c = server_.resolve_entity.connect(/*0, */ std::bind(&userdir_plugin::resolve_entity, this, _1));

		srv.register_cvar("UserDir", x0::context::server | x0::context::vhost, std::bind(&userdir_plugin::setup_userdir, this, _1, _2));
	}

	~userdir_plugin()
	{
		server_.resolve_entity.disconnect(c);
	}

	bool setup_userdir(const x0::settings_value& cvar, x0::scope& s)
	{
		std::string dirname;
		if (cvar.load(dirname) && validate(dirname))
		{
			s.acquire<context>(this)->dirname_ = dirname;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool validate(std::string& path)
	{
		if (path.empty())
			return false;

		if (path[0] == '/')
			return false;

		path.insert(0, "/");

		if (path[path.size() - 1] == '/')
			path = path.substr(0, path.size() - 1);

		return true;
	}

private:
	void resolve_entity(x0::request *in)
	{
		auto cx = server_.vhost(in->hostid()).get<context>(this);
		if (!cx || cx->dirname_.empty())
			return;

		if (in->path.size() <= 2 || in->path[1] != '~')
			return;

		const std::size_t i = in->path.find("/", 2);
		std::string userName, userPath;

		if (i != std::string::npos)
		{
			userName = in->path.substr(2, i - 2);
			userPath = in->path.substr(i);
		}
		else
		{
			userName = in->path.substr(2);
			userPath = "";
		}

		if (struct passwd *pw = getpwnam(userName.c_str()))
		{
			in->document_root = pw->pw_dir + cx->dirname_;
			in->fileinfo = server_.fileinfo(in->document_root + userPath);
			debug(0, "docroot[%s], fileinfo[%s]", in->document_root.c_str(), in->fileinfo->filename().c_str());
		}
	}
};

X0_EXPORT_PLUGIN(userdir);
