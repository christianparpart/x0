/* <x0/plugins/userdir.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpHeader.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <pwd.h>

/**
 * \ingroup plugins
 * \brief implements automatic index file resolving, if mapped request path is a path.
 */
class userdir_plugin :
	public x0::HttpPlugin
{
private:
	x0::HttpServer::RequestHook::Connection c;

	struct context : public x0::ScopeValue
	{
		std::string dirname_;

		virtual void merge(const x0::ScopeValue *value)
		{
			if (auto cx = dynamic_cast<const context *>(value))
			{
				if (dirname_.empty())
					dirname_ = cx->dirname_;
			}
		}
	};

public:
	userdir_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		using namespace std::placeholders;
		c = server_.onResolveEntity.connect<userdir_plugin, &userdir_plugin::resolve_entity>(this);

		declareCVar("UserDir", x0::HttpContext::server | x0::HttpContext::host, &userdir_plugin::setup_userdir);
	}

	~userdir_plugin()
	{
		server_.onResolveEntity.disconnect(c);
	}

	std::error_code setup_userdir(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		std::string dirname;
		std::error_code ec = cvar.load(dirname);
		if (ec)
			return ec;

		ec = validate(dirname);
		if (ec)
			return ec;

		s.acquire<context>(this)->dirname_ = dirname;
		return std::error_code();
	}

	std::error_code validate(std::string& path)
	{
		if (path.empty())
			return std::make_error_code(std::errc::invalid_argument);

		if (path[0] == '/')
			return std::make_error_code(std::errc::invalid_argument);

		path.insert(0, "/");

		if (path[path.size() - 1] == '/')
			path = path.substr(0, path.size() - 1);

		return std::error_code();
	}

private:
	void resolve_entity(x0::HttpRequest *in)
	{
		auto cx = server_.resolveHost(in->hostid())->get<context>(this);
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
