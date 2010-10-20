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
	std::string dirname_;

public:
	userdir_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name),
		dirname_("/public_html")
	{
		registerSetupFunction<userdir_plugin, &userdir_plugin::setup_userdir>("userdir.name", Flow::Value::VOID);
		registerFunction<userdir_plugin, &userdir_plugin::handleRequest>("userdir", Flow::Value::VOID);
	}

	~userdir_plugin()
	{
	}

	void setup_userdir(Flow::Value& result, const x0::Params& args)
	{
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
	void handleRequest(Flow::Value& result, x0::HttpRequest *in, x0::HttpResponse *out, const x0::Params& args)
	{
		if (dirname_.empty())
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
			in->document_root = pw->pw_dir + dirname_;
			in->fileinfo = in->connection.worker().fileinfo(in->document_root + userPath);
			debug(0, "docroot[%s], fileinfo[%s]", in->document_root.c_str(), in->fileinfo->filename().c_str());
		}
	}
};

X0_EXPORT_PLUGIN(userdir)
