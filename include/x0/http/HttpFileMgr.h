/* <HttpFileMgr.h>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/http/HttpFile.h>
#include <x0/http/HttpFileRef.h>
#include <ev++.h>

namespace x0 {

class X0_API HttpFileMgr {
public:
	struct Settings;

	HttpFileMgr(ev::loop_ref loop, Settings* settings);
	~HttpFileMgr();

	HttpFileRef query(const std::string& path);

	HttpFileRef get(const std::string& path) { return query(path); }
	HttpFileRef operator()(const std::string& path) { return query(path); }
	HttpFileRef operator[](const std::string& path) { return query(path); }

private:
	ev::loop_ref loop_;
	const Settings* settings_;
	std::unordered_map<std::string, HttpFileRef> files_;

	friend class HttpFile;
};

struct X0_API HttpFileMgr::Settings {
	bool etagConsiderMtime;
	bool etagConsiderSize;
	bool etagConsiderInode;

	unsigned cacheTTL;

	std::unordered_map<std::string, std::string> mimetypes;
	std::string defaultMimetype;

	explicit Settings(const std::string& mimefile = "") :
		etagConsiderMtime(true),
		etagConsiderSize(true),
		etagConsiderInode(false),
		cacheTTL(10),
		mimetypes(),
		defaultMimetype("text/plain")
	{
		if (!mimefile.empty()) {
			openMimeTypes(mimefile);
		}
	}

	bool openMimeTypes(const std::string& filename);
};

} // namespace x0
