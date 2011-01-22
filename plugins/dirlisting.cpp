/* <x0/plugins/dirlisting.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: content generator
 *
 * description:
 *     Generates a directory listing response if the requested
 *     path (its physical target file) is a directory.
 *
 * setup API:
 *     none
 *
 * request processing API:
 *     handler dirlisting();
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
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
public:
	dirlisting_plugin(x0::HttpServer& srv, const std::string& name) :
		x0::HttpPlugin(srv, name)
	{
		registerHandler<dirlisting_plugin, &dirlisting_plugin::handleRequest>("dirlisting");
	}

private:
	bool handleRequest(x0::HttpRequest *in, const x0::Params& args)
	{
		if (!in->fileinfo->isDirectory())
			return false;

		DIR *dir = opendir(in->fileinfo->filename().c_str());
		if (!dir)
			return false;

		x0::Buffer result(mkhtml(dir, in));

		closedir(dir);

		in->status = x0::HttpError::Ok;
		in->responseHeaders.push_back("Content-Type", "text/html");
		in->responseHeaders.push_back("Content-Length", boost::lexical_cast<std::string>(result.size()));

		in->write(
			std::make_shared<x0::BufferSource>(std::move(result)),
			std::bind(&x0::HttpRequest::finish, in)
		);
		return true;
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
				if (x0::FileInfoPtr fi = in->connection.worker().fileinfo(in->fileinfo->filename() + "/" + name))
				{
					if (fi->isDirectory())
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

		sstr << "<h2 style='font-family: Courier New, monospace;'>Index of " << in->path.str() << "</h2>\n";
		sstr << "<br/><ul>\n";

		for (std::list<std::string>::iterator i = listing.begin(), e = listing.end(); i != e; ++i)
		{
			sstr << "<li><a href='" << *i << "'>" << *i << "</a></li>\n";
		}

		sstr << "</ul>\n";

		sstr << "<hr/>\n";
		sstr << "<small><pre>" << in->connection.worker().server().tag() << "</pre></small><br/>\n";

		sstr << "</body></html>\n";

		return sstr.str();
	}
};

X0_EXPORT_PLUGIN(dirlisting)
