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

	std::string global_;

public:
	userdir_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		using namespace std::placeholders;
		c = server_.resolve_entity.connect(/*0, */ std::bind(&userdir_plugin::resolve_entity, this, _1));

		srv.register_cvar_server("UserDir", std::bind(&userdir_plugin::setup_userdir_default, this, _1));
		srv.register_cvar_host("UserDir", std::bind(&userdir_plugin::setup_userdir_host, this, _1, _2));
	}

	~userdir_plugin()
	{
		server_.resolve_entity.disconnect(c);
	}

	void setup_userdir_default(const x0::settings_value& cvar)
	{
		std::string userdir;

		if (cvar.load(userdir) && validate(userdir))
		{
			global_ = userdir;
		}
		debug(0, "userdir_default: '%s'", userdir.c_str());
	}

	void setup_userdir_host(const x0::settings_value& cvar, const std::string& hostid)
	{
		std::string userdir;

		if (cvar.load(userdir) && validate(userdir))
		{
			*server_.create_context<std::string>(this, hostid) = userdir;
		}
		debug(0, "userdir_host[%s]: '%s'", hostid.c_str(), userdir.c_str());
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
		const std::string *userdir = server_.context<std::string>(this, in->hostid());
		debug(0, "userdir[per_host]: %p (%s)", userdir, userdir ? userdir->c_str() : "");
		if (!userdir)
		{
			userdir = &global_;
			debug(0, "userdir[server]: %p (%s)", userdir, userdir->c_str());
			if (userdir->empty())
				return;
		}

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
		debug(0, "name[%s], path[%s]", userName.c_str(), userPath.c_str());

		if (struct passwd *pw = getpwnam(userName.c_str()))
		{
			in->document_root = pw->pw_dir + *userdir;
			in->fileinfo = server_.fileinfo(in->document_root + userPath);
			debug(0, "docroot[%s], fileinfo[%s]",
					in->document_root.c_str(), in->fileinfo->filename().c_str());
		}
	}
};

X0_EXPORT_PLUGIN(userdir);
