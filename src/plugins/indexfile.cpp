/* <x0/plugins/indexfile.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include "indexfile.h"
#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpHeader.h>
#include <x0/Scope.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void indexfile_plugin::context::merge(const x0::ScopeValue *value)
{
	if (auto cx = dynamic_cast<const context *>(value))
	{
		if (index_files.empty())
			index_files = cx->index_files;
	}
}

indexfile_plugin::indexfile_plugin(x0::HttpServer& srv, const std::string& name) :
	x0::HttpPlugin(srv, name)
{
	// to connect to resolved_entity at slot-group `1`, so, that all other transforms have taken place already,
	// that is, e.g. "userdir".
	// XXX a better implementation of this dependency-issue surely is, to introduce
	// another signal that would order the event sequence for us, but i'm not yet that clear about how
	// to name this in a clean and reasonable way.
	using namespace std::placeholders;
	c = server_.resolve_entity.connect(/*FIXME 1, */ std::bind(&indexfile_plugin::indexfile, this, _1));

	server_.declareCVar("IndexFiles", x0::HttpContext::server | x0::HttpContext::host, std::bind(&indexfile_plugin::setup_indexfiles, this, _1, _2));
}

indexfile_plugin::~indexfile_plugin()
{
	server_.resolve_entity.disconnect(c);
	server_.release(this);
}

std::vector<std::string> *indexfile_plugin::indexFiles(const x0::Scope& scope) const
{
	if (context *ctx = scope.get<context>(this))
		return &ctx->index_files;

	return 0;
}

void indexfile_plugin::setIndexFiles(x0::Scope& scope, const std::vector<std::string>& indexFiles)
{
	scope.acquire<context>(this)->index_files = indexFiles;
}

bool indexfile_plugin::setup_indexfiles(const x0::SettingsValue& cvar, x0::Scope& s)
{
	return cvar.load(s.acquire<context>(this)->index_files);
}

void indexfile_plugin::indexfile(x0::HttpRequest *in)
{
	if (!in->fileinfo->is_directory())
		return;

	auto files = indexFiles(server_.host(in->hostid()));
	if (!files)
		return;

	std::string path(in->fileinfo->filename());

	for (auto i = files->begin(), e = files->end(); i != e; ++i)
	{
		std::string ipath;
		ipath.reserve(path.length() + 1 + i->length());
		ipath += path;
		if (path[path.size() - 1] != '/')
			ipath += "/";
		ipath += *i;

		if (x0::FileInfoPtr fi = in->connection.server().fileinfo(ipath))
		{
			if (fi->is_regular())
			{
				in->fileinfo = fi;
				break;
			}
		}
	}
}

X0_EXPORT_PLUGIN(indexfile);
