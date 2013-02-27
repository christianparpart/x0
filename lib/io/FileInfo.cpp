/* <x0/FileInfo.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/io/FileInfo.h>
#include <x0/io/FileInfoService.h>

namespace x0 {

FileInfo::FileInfo(FileInfoService& service, const std::string& path) :
	service_(service),
	stat_(),
	errno_(0),
	inotifyId_(-1),
	cachedAt_(ev_now(service.loop_)),
	path_(path),
	etag_(),
	mtime_(),
	mimetype_()
{
	if (update()) {
		etag_ = service_.make_etag(*this);
		mimetype_ = service_.get_mimetype(path_);
	}
}

bool FileInfo::update()
{
	if (::stat(path_.c_str(), &stat_) < 0) {
		errno_ = errno;
		return false;
	}
	return true;
}

void FileInfo::clear()
{
	clearCustomData();

	etag_ = service_.make_etag(*this);
	mtime_.clear(); // gets computed on-demand
	mimetype_ = service_.get_mimetype(path_);
}

} // namespace x0
