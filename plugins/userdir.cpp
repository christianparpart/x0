/* <x0/plugins/userdir.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: mapper
 *
 * description:
 *     Maps request path to a local file within the user's home directory.
 *
 * setup API:
 *     void userdir.name(string);
 *
 * request processing API:
 *     void userdir();
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
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
	std::string dirname_;

public:
	userdir_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		dirname_("/public_html")
	{
		registerSetupProperty<userdir_plugin, &userdir_plugin::setup_userdir>("userdir.name", x0::FlowValue::STRING);
		registerFunction<userdir_plugin, &userdir_plugin::handleRequest>("userdir", x0::FlowValue::VOID);
	}

	~userdir_plugin()
	{
	}

	void setup_userdir(x0::FlowValue& result, const x0::FlowParams& args)
	{
		if (args.empty()) {
			result.set(dirname_.c_str());
			return;
		}

		std::string dirname;
		if (!args[0].load(dirname))
			return;

		std::error_code ec = validate(dirname);
		if (ec) {
			server().log(x0::Severity::error, "userdir: %s", ec.message().c_str());
			return;
		}

		dirname_ = dirname;
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
	void handleRequest(x0::FlowValue& result, x0::HttpRequest *r, const x0::FlowParams& args)
	{
		if (dirname_.empty())
			return;

		if (r->path.size() <= 2 || r->path[1] != '~')
			return;

		const std::size_t i = r->path.find("/", 2);
		std::string userName, userPath;

		if (i != std::string::npos)
		{
			userName = r->path.substr(2, i - 2);
			userPath = r->path.substr(i);
		}
		else
		{
			userName = r->path.substr(2);
			userPath = "";
		}

		if (struct passwd *pw = getpwnam(userName.c_str()))
		{
			r->documentRoot = pw->pw_dir + dirname_;
			r->fileinfo = r->connection.worker().fileinfo(r->documentRoot + userPath);
			//debug(0, "docroot[%s], fileinfo[%s]", r->documentRoot.c_str(), r->fileinfo->filename().c_str());
		}
	}
};

X0_EXPORT_PLUGIN(userdir)
