/* <x0/plugins/dirlisting.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: content generator
 *
 * description:
 *     Generates a directory listing response if the requested
 *     path (its physical target file) is a directory.
 *
 * setup handlers / functions API:
 *     none
 *
 * request handler API:
 *     handler dirlisting();            # generates a simple table view
 *     handler dirlisting.google();     # generates an advanced google-based table view
 *
 * request function API:
 *     none
 *
 *
 * TODO: add HTML sanitizing of file/path names.
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

#include <cstdio>
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
		registerHandler<dirlisting_plugin, &dirlisting_plugin::simple>("dirlisting");
		registerHandler<dirlisting_plugin, &dirlisting_plugin::google>("dirlisting.google");
	}

private:
	bool simple(x0::HttpRequest *in, const x0::FlowParams& args)
	{
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

		int rv = dirlisting(in->fileinfo, in->connection.worker().fileinfo, [&](x0::FileInfoPtr file) {
			sstr << "\t<tr>\n";
			if (file->isDirectory()) {
				sstr << "\t\t<td class='subdir' colspan='2'><a href='"
					 << file->filename() << "/'>" << file->filename() << "</a></td>\n"
					 << "\t\t<td class='mimetype'>directory</td>"
					 << "</td>\n";
			} else {
				sstr << "\t\t<td class='name'><a href='" << file->filename() << "'>"
					 << file->filename() << "</a></td>\n";
				sstr << "\t\t<td class='size'>" << file->size() << "</td>\n";
				sstr << "\t\t<td class='mimetype'>" << file->mimetype() << "</td>\n";
			}
			sstr << "\t</tr>\n";
		});

		if (!rv)
			return false;

		sstr << "</table>\n";
		sstr << "<hr/>\n";
		sstr << "<small><pre>" << in->connection.worker().server().tag() << "</pre></small><br/>\n";
		sstr << "</body></html>\n";

		in->status = x0::HttpStatus::Ok;
		in->responseHeaders.push_back("Content-Type", "text/html");
		in->responseHeaders.push_back("Content-Length", boost::lexical_cast<std::string>(sstr.size()));

		in->write<x0::BufferSource>(std::move(sstr));
		in->finish();

		return true;
	}

	bool google(x0::HttpRequest* in, const x0::FlowParams& args)
	{
		x0::Buffer buf;

		buf << "<html>\n"
				"<head>\n"
				"<style>\n"
				"a.link { display: block; }\n"
				"</style>\n"
				"<script type='text/javascript' src='https://www.google.com/jsapi'></script>\n"
				"<script type='text/javascript'>\n"
				"google.load('visualization', '1', {packages:['table']});\n"
				"google.setOnLoadCallback(drawTable);\n"
				"function drawTable() {\n";

		buf <<  "var data = new google.visualization.DataTable();\n"
				"data.addColumn('string', 'File Name');\n"
				"data.addColumn('number', 'File Size');\n"
				"data.addColumn('datetime', 'Last Modified');\n"
				"data.addColumn('string', 'Mime Type');\n"
				"data.addColumn('number', 'is-directory');\n";

		int rv = dirlisting(in->fileinfo, in->connection.worker().fileinfo, [&](x0::FileInfoPtr file) {
			buf << "data.addRow(['"
				<< file->filename();

			if (file->isDirectory())
				buf << '/';

			buf << "', "
				<< file->size() << ", "
				<< "new Date(" << file->mtime() << "*1000), '"
				<< (!file->isDirectory() ? file->mimetype() : "") << "', "
				<< (file->isDirectory() ? 1 : 0)
				<< "]);\n";
		});

		if (!rv)
			return false;

		buf << "var linkFormatter = new google.visualization.PatternFormat('<a class=\"link\" href=\"{0}\">{0}</a>');\n";
		buf << "linkFormatter.format(data, [0]);\n";

		buf << "var timeFormatter = new google.visualization.DateFormat({ pattern: 'yyyy-MM-d HH:mm:ss' });\n";
		buf << "timeFormatter.format(data, 2);\n";

		buf << "data.sort([{column: 3}, {column: 0}]);\n";

		buf << "var view = new google.visualization.DataView(data);\n"
				"view.setColumns([0, 1, 2, 3]);\n";

		buf << "var table = new google.visualization.Table(document.getElementById('table_div'));\n"
				"table.draw(view, {allowHtml: true, showRowNumber: true});\n"
				"}\n"
				"</script>\n"
				"</head>\n"
				"<body>\n";

		buf << "<h1>Directory listing of: " << in->path << "</h1>\n";

		buf << "<div id='table_div'></div>\n";
		buf << "<hr/>\n";
		buf << "<small><pre>" << in->connection.worker().server().tag() << "</pre></small><br/>\n";
		buf << "</body></html>\n";

		// get string version of content length
		char slen[16];
		snprintf(slen, sizeof(slen), "%zu", buf.size());

		in->status = x0::HttpStatus::Ok;
		in->responseHeaders.push_back("Content-Type", "text/html");
		in->responseHeaders.push_back("Content-Length", slen);
		in->write<x0::BufferSource>(std::move(buf));
		in->finish();
		return true;
	}

	bool dirlisting(x0::FileInfoPtr fi, x0::FileInfoService& fis, std::function<void(x0::FileInfoPtr)> callback)
	{
		if (!fi)
			return false;

		if (!fi->isDirectory())
			return false;

		// alloc some resources
		DIR* dir = opendir(fi->path().c_str());
		if (!dir)
			return false;

		int len = offsetof(dirent, d_name) + pathconf(fi->path().c_str(), _PC_NAME_MAX);
		dirent* dep = (dirent*) new unsigned char [len + 1];
		dirent* res = nullptr;
		x0::Buffer buf;

		x0::Buffer filename;
		filename << fi->path() << '/';
		std::size_t baseFileNameLength = filename.size();

		while (readdir_r(dir, dep, &res) == 0 && res) {
			// skip '.'
			if (dep->d_name[0] == '.' && dep->d_name[1] == 0)
				continue;

			// prepare filename
			filename.push_back(static_cast<char*>(dep->d_name));

			// send fileinfo struct to caller, if possible
			if (x0::FileInfoPtr fi = fis.query(filename.str()))
				callback(fi);

			// reset filename to its base-length
			filename.resize(baseFileNameLength);
		}

		// free resources
		delete[] dep;
		closedir(dir);

		return true;
	}
};

X0_EXPORT_PLUGIN(dirlisting)
