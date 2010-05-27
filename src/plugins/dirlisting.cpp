/* <x0/mod_dirlisting.cpp>
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
#include <x0/io/buffer_source.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>

#include <boost/lexical_cast.hpp>

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
	public x0::plugin
{
private:
	struct context
	{
		bool enabled;

		context() : enabled(false) {}
	};

	x0::request_handler::connection c;
	context default_;

public:
	dirlisting_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name),
		default_()
	{
		c = server_.generate_content.connect(&dirlisting_plugin::dirlisting, this);

		using namespace std::placeholders;
		server_.register_cvar_server("DirectoryListing", std::bind(&dirlisting_plugin::setup_dirlisting_server, this, _1));
		server_.register_cvar_host("DirectoryListing", std::bind(&dirlisting_plugin::setup_dirlisting_host, this, _1, _2));
	}

	~dirlisting_plugin()
	{
		c.disconnect();
	}

private:
	void setup_dirlisting_server(const x0::settings_value& cvar)
	{
		cvar.load(default_.enabled);
	}

	void setup_dirlisting_host(const x0::settings_value& cvar, const std::string& hostid)
	{
		bool enabled;
		if (!cvar.load(enabled))
			return;

		server_.create_context<context>(this, hostid)->enabled = enabled;
	}

	void dirlisting(x0::request_handler::invokation_iterator next, x0::request *in, x0::response *out)
	{
		context *ctx = server_.context<context>(this, in->hostid());
		if (!ctx)
			ctx = &default_;

		if (!ctx->enabled)
			return next();

		if (!in->fileinfo->is_directory())
			return next();

		if (DIR *dir = opendir(in->fileinfo->filename().c_str()))
		{
			x0::buffer result(mkhtml(dir, in));

			closedir(dir);

			out->status = x0::http_error::ok;
			out->headers.push_back("Content-Type", "text/html");
			out->headers.push_back("Content-Length", boost::lexical_cast<std::string>(result.size()));

			return out->write(
				std::make_shared<x0::buffer_source>(std::move(result)),
				std::bind(&dirlisting_plugin::done, this, next)
			);
		}
		return next();
	}

	void done(x0::request_handler::invokation_iterator next)
	{
		next.done();
	}

	std::string mkhtml(DIR *dir, x0::request *in)
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
				if (x0::fileinfo_ptr fi = in->connection.server().fileinfo(in->fileinfo->filename() + "/" + name))
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
