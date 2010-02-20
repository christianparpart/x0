/* <x0/mod_dirlisting.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/server.hpp>
#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/header.hpp>
#include <x0/io/buffer_source.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
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
 */
class dirlisting_plugin :
	public x0::plugin
{
private:
	x0::handler::connection c;

	struct context
	{
		bool enabled;
		std::string xsluri;
	};

public:
	dirlisting_plugin(x0::server& srv, const std::string& name) :
		x0::plugin(srv, name)
	{
		c = server_.generate_content.connect(boost::bind(&dirlisting_plugin::dirlisting, this, _1, _2));
	}

	~dirlisting_plugin()
	{
		server_.generate_content.disconnect(c);
	}

	virtual void configure()
	{
		std::string default_uri;
		server_.config()["DirectoryListing"]["XslUri"].load(default_uri);

		auto hosts = server_.config()["Hosts"].keys<std::string>();
		for (auto i = hosts.begin(), e = hosts.end(); i != e; ++i)
		{
			bool enabled;

			if (server_.config()["Hosts"][*i]["DirectoryListing"]["Enabled"].load(enabled)
			 || server_.config()["DirectoryListing"]["Enabled"].load(enabled))
			{
				context& ctx = server_.create_context<context>(this, *i);
				ctx.enabled = enabled;
				ctx.xsluri = server_.config()["Hosts"][*i]["DirectoryListing"]["XslUri"].get(default_uri);
			}
		}
	}

private:
	bool dirlisting(x0::request& in, x0::response& out)
	{
		try
		{
			context& ctx = server_.context<context>(this, in.hostid());

			if (!ctx.enabled)
				return false;

			if (!in.fileinfo->is_directory())
				return false;

			if (DIR *dir = opendir(in.fileinfo->filename().c_str()))
			{
				bool xml = !ctx.xsluri.empty();
				x0::buffer result(xml ? mkxml(dir, ctx, in) : mkplain(dir, in));

				out *= x0::response_header("Content-Type", xml ? "text/xml" : "text/html");
				out *= x0::response_header("Content-Length", boost::lexical_cast<std::string>(result.size()));

				closedir(dir);

				out.write(x0::source_ptr(new x0::buffer_source(result)),
					boost::bind(&dirlisting_plugin::done, this, out));

				return true;
			}
		}
		catch (const x0::context::not_found_error&)
		{
			// eat up and default to `unhandled`
		}
		return false;
	}

	void done(x0::response& out)
	{
		out.finish();
	}

	std::string mkplain(DIR *dir, x0::request& in)
	{
		std::list<std::string> listing;
		listing.push_back("..");

		int len = offsetof(dirent, d_name) + pathconf(in.fileinfo->filename().c_str(), _PC_NAME_MAX);
		dirent *dep = (dirent *)new unsigned char[len + 1];
		dirent *res = 0;

		while (readdir_r(dir, dep, &res) == 0 && res)
		{
			std::string name(dep->d_name);

			if (name[0] != '.')
			{
				if (x0::fileinfo_ptr fi = in.connection.server().fileinfo(in.fileinfo->filename() + "/" + name))
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
			 << in.path
			 << "</title></head>\n<body>\n";

		sstr << "<h2>Index of " << in.path << "</h2>\n";
		sstr << "<br/><ul>\n";

		for (std::list<std::string>::iterator i = listing.begin(), e = listing.end(); i != e; ++i)
		{
			sstr << "<li><a href='" << *i << "'>" << *i << "</a></li>\n";
		}

		sstr << "</ul>\n";

		sstr << "<hr/>\n";
		sstr << "<small><i>" << in.connection.server().tag() << "</i></small><br/>\n";

		sstr << "</body></html>\n";

		return sstr.str();
	}

	std::string mkxml(DIR *dir, context& ctx, x0::request& in)
	{
		std::stringstream xml;

		xml << "<?xml version='1.0' encoding='" << "utf-8" << "'?>\n";

		if (!ctx.xsluri.empty())
			xml << "<?xml-stylesheet type='text/xsl' href='" << ctx.xsluri << "'?>\n";

		xml << "<dirlisting path='" << in.path.str() << "' tag='" << server_.tag() << "'>\n";

		int len = offsetof(dirent, d_name) + pathconf(in.fileinfo->filename().c_str(), _PC_NAME_MAX);
		dirent *dep = (dirent *)new unsigned char[len + 1];
		dirent *res = 0;

		while (readdir_r(dir, dep, &res) == 0 && res)
		{
			std::string name(dep->d_name);

			if (name[0] == '.')
				continue;

			if (x0::fileinfo_ptr fi = in.connection.server().fileinfo(in.fileinfo->filename() + "/" + name))
			{
				if (fi->is_directory())
					name += "/";

				xml << "<item name='" << name
					<< "' size='" << fi->size()
					<< "' mtime='" << fi->last_modified()
					<< "' mimetype='" << fi->mimetype()
					<< "' />\n";
			}
		}

		xml << "</dirlisting>\n";

		return xml.str();
	}
};

X0_EXPORT_PLUGIN(dirlisting);
