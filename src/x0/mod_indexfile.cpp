/* <x0/mod_indexfile.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
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

/**
 * \ingroup modules
 * \brief implements automatic index file resolving, if mapped request path is a path.
 */
class indexfile_plugin :
	public x0::plugin
{
private:
	boost::signals::connection c;
	std::vector<std::string> index_files;

public:
	indexfile_plugin(x0::server& srv) :
		x0::plugin(srv)
	{
		c = server_.resolve_entity.connect(boost::bind(&indexfile_plugin::indexfile, this, _1));
	}

	~indexfile_plugin()
	{
		server_.resolve_entity.disconnect(c);
	}

	virtual void configure()
	{
		index_files.push_back("index.html");
		index_files.push_back("index.htm");
		// TODO retrieve file to store indexfile log to.
	}

private:
	void indexfile(x0::request& in)
	{
		std::string path(in.document_root + in.path);
		struct stat st;

		if (stat(path.c_str(), &st) != 0) return;

		if (!S_ISDIR(st.st_mode)) return;

		for (auto i = index_files.begin(), e = index_files.end(); i != e; ++i)
		{
			std::string ipath;
			ipath.reserve(path.length() + 1 + i->length());
			ipath += path;
			if (path[path.size() - 1] != '/')
				ipath += "/";
			ipath += *i;

			struct stat st2;
			if (stat(ipath.c_str(), &st2) == 0 && S_ISREG(st2.st_mode))
			{
				in.entity = ipath;
				break;
			}
		}
	}
};

extern "C" void indexfile_init(x0::server& srv) {
	srv.setup_plugin(x0::plugin_ptr(new indexfile_plugin(srv)));
}
