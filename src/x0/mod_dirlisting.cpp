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
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stddef.h>

/**
 * \ingroup modules
 * \brief implements automatic content generation for raw directories
 */
class dirlisting_plugin :
	public x0::plugin
{
private:
	x0::handler::connection c;

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
		// TODO get some kind of solution to let dir-listing only be used for directories we allow it for.
	}

private:
	bool dirlisting(x0::request& in, x0::response& out)
	{
		if (!x0::isdir(in.entity)) return false;

		if (DIR *dir = opendir(in.entity.c_str()))
		{
			std::list<std::string> listing;
			listing.push_back("..");

			int len = offsetof(dirent, d_name) + pathconf(in.entity.c_str(), _PC_NAME_MAX);
			dirent *dep = (dirent *)new unsigned char[len + 1];
			dirent *res = 0;

			while (readdir_r(dir, dep, &res) == 0 && res)
			{
				std::string name(dep->d_name);

				if (name[0] != '.')
				{
					if (x0::isdir(in.entity + name)) name += "/";

					listing.push_back(name);
				}
			}

			delete[] dep;

			std::stringstream sstr;
			sstr << "<html><head><title>Directory: "
				 << in.path
				 << "</title></head>\n<body>\n";

			sstr << "<h2>Index of " << in.path << "</h2>\n";
			sstr << "<ul>\n";

			for (std::list<std::string>::iterator i = listing.begin(), e = listing.end(); i != e; ++i)
			{
				sstr << "<li><a href='" << *i << "'>" << *i << "</a></li>\n";
			}

			sstr << "</ul>\n</body></html>\n";

			std::string result(sstr.str());

			out.write(result);
			out *= x0::header("Content-Type", "text/html");
			out *= x0::header("Content-Length", boost::lexical_cast<std::string>(result.size()));

			out.flush();

			return true;
		}
		return false;
	}
};

X0_EXPORT_PLUGIN(dirlisting);
