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
		std::list<std::pair<std::string, x0::FileInfoPtr>> listing;

		if (!x0::equals(in->path, "/"))
			listing.push_back(std::make_pair("..", in->fileinfo));

		int len = offsetof(dirent, d_name) + pathconf(in->fileinfo->filename().c_str(), _PC_NAME_MAX);
		dirent *dep = (dirent *)new unsigned char[len + 1];
		dirent *res = 0;

		while (readdir_r(dir, dep, &res) == 0 && res) {
			if (dep->d_name[0] != '.') {
				std::string name(dep->d_name);
				if (x0::FileInfoPtr fi = in->connection.worker().fileinfo(in->fileinfo->filename() + "/" + name)) {
					listing.push_back(std::make_pair(name, fi));
				}
			}
		}
		delete[] dep;

		x0::Buffer sstr;
		sstr << "<html><head><title>Directory: " << in->path << "</title>";
		sstr << "<style>\n"
			"\tthead { font-weight: bold; }\n"
			"\ttd.name { width: 200px; }\n"
			"\ttd.size { width: 80px; }\n"
			"\ttd.subdir { width: 280px; }\n"
			"\ttd.mimetype { }\n"
			"\ttr:hover { background-color: #EEE; }\n"
			"</style>\n";
		sstr << "</head>\n";
		sstr << "<body>\n";

		sstr << "<h2 style='font-family: Courier New, monospace;'>Index of " << in->path.str() << "</h2>\n";
		sstr << "<br/>";
		sstr << "<table>\n";

		sstr << "<thead>"
			    "<td class='name'>Name</td>"
			    "<td class='size'>Size</td>"
			    "<td class='mimetype'>Mime type</td>"
			    "</thead>\n";

		for (auto i: listing) {
			sstr << "\t<tr>\n";
			if (i.second->isDirectory()) {
				sstr << "\t\t<td class='subdir' colspan='2'><a href='"
					 << i.first << "/'>" << i.first << "</a>"
					 << "\t\t<td class='mimetype'>directory</td>"
					 << "</td>\n";
			} else {
				sstr << "\t\t<td class='name'><a href='" << i.first  << "'>"
					 << i.first << "</a></td>\n";
				sstr << "\t\t<td class='size'>" << (int)i.second->size() << "</td>\n";
				sstr << "\t\t<td class='mimetype'>" << i.second->mimetype() << "</td>\n";
			}
			sstr << "\t</tr>";
		}

		sstr << "</table>\n";

		sstr << "<hr/>\n";
		sstr << "<small><pre>" << in->connection.worker().server().tag() << "</pre></small><br/>\n";

		sstr << "</body></html>\n";

		return sstr.str();
	}
};

X0_EXPORT_PLUGIN(dirlisting)
