/* <x0/plugins/dirlisting.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/strutils.h>
#include <x0/Types.h>

#include <boost/lexical_cast.hpp>
#include <boost/logic/tribool.hpp>

#include <cstring>
#include <cerrno>
#include <cstddef>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

/**
 * \ingroup plugins
 * \brief implements automatic content generation for raw directories
 *
 * \todo cache page objects for later reuse.
 * \todo add template-support (LUA based)
 * \todo allow config overrides: server/vhost/location
 */
class dirlisting_plugin :
	public x0::HttpPlugin
{
private:
	x0::request_handler::connection c;

	struct context : public x0::ScopeValue
	{
		boost::tribool enabled; // make it a tribool to introduce "undefined"?

		context() :
			enabled(boost::indeterminate)
		{
		}

		virtual void merge(const x0::ScopeValue *value)
		{
			if (auto cx = dynamic_cast<const context *>(value))
			{
				if (enabled == boost::indeterminate)
				{
					enabled = cx->enabled;
				}
			}
		}
	};

public:
	dirlisting_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		c = server_.generate_content.connect(&dirlisting_plugin::dirlisting, this);

		using namespace std::placeholders;
		declareCVar("DirectoryListing", x0::HttpContext::server | x0::HttpContext::host, &dirlisting_plugin::setup_dirlisting);
	}

	~dirlisting_plugin()
	{
		c.disconnect();
	}

private:
	bool setup_dirlisting(const x0::SettingsValue& cvar, x0::Scope& s)
	{
		return cvar.load(s.acquire<context>(this)->enabled);
	}

	void dirlisting(x0::request_handler::invokation_iterator next, x0::HttpRequest *in, x0::HttpResponse *out)
	{
		if (!in->fileinfo->is_directory())
			return next();

		context *ctx = server_.host(in->hostid()).get<context>(this);
//		if (!ctx)
//			ctx = server_.get<context>(this);

		if (ctx && ctx->enabled == true)
			return process(next, in, out);
		else
			return next();
	}

	void process(x0::request_handler::invokation_iterator next, x0::HttpRequest *in, x0::HttpResponse *out)
	{
		debug(0, "process: %s [%s]",
			in->fileinfo->filename().c_str(),
			in->document_root.c_str());

		if (DIR *dir = opendir(in->fileinfo->filename().c_str()))
		{
			x0::Buffer result(mkhtml(dir, in));

			closedir(dir);

			out->status = x0::http_error::ok;
			out->headers.push_back("Content-Type", "text/html");
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(result.size()));

			return out->write(
				std::make_shared<x0::BufferSource>(std::move(result)),
				std::bind(&dirlisting_plugin::done, this, next)
			);
		}
		else
		{
			return next();
		}
	}

	void done(x0::request_handler::invokation_iterator next)
	{
		next.done();
	}

	std::string mkhtml(DIR *dir, x0::HttpRequest *in)
	{
		std::list<std::string> listing;
		listing.push_back("..");

		int len = offsetof(dirent, d_name) + pathconf(in->fileinfo->filename().c_str(), _PC_NAME_MAX);
		dirent *dep = (dirent *)new unsigned char[len + 1];
		dirent *res = 0;

		while (readdir_r(dir, dep, &res) == 0 && res)
		{
			std::string name(dep->d_name);

			if (name[0] != '.')
			{
				if (x0::FileInfoPtr fi = in->connection.server().fileinfo(in->fileinfo->filename() + "/" + name))
				{
					if (fi->is_directory())
						name += "/";

					listing.push_back(name);
				}
			}
		}

		delete[] dep;

		std::stringstream sstr;
		sstr << "<html><head><title>Directory: "
			 << in->path.str()
			 << "</title></head>\n<body>\n";

		sstr << "<h2>Index of " << in->path.str() << "</h2>\n";
		sstr << "<br/><ul>\n";

		for (std::list<std::string>::iterator i = listing.begin(), e = listing.end(); i != e; ++i)
		{
			sstr << "<li><a href='" << *i << "'>" << *i << "</a></li>\n";
		}

		sstr << "</ul>\n";

		sstr << "<hr/>\n";
		sstr << "<small><i>" << in->connection.server().tag() << "</i></small><br/>\n";

		sstr << "</body></html>\n";

		return sstr.str();
	}
};

X0_EXPORT_PLUGIN(dirlisting);
