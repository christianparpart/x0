/* <x0/FileInfo.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FileInfo.h>
#include <x0/io/FileInfoService.h>

namespace x0 {

FileInfo::FileInfo(FileInfoService& service, const std::string& filename) :
	service_(service),
	stat_(),
	inotifyId_(-1),
	cachedAt_(ev_now(service.loop_)),
	filename_(filename),
	exists_(false),
	etag_(),
	mtime_(),
	mimetype_()
{
	if (filename_.empty())
		return;

	if (::stat(filename_.c_str(), &stat_) == 0)
	{
		exists_ = true;
		etag_ = service_.make_etag(*this);
		mimetype_ = service_.get_mimetype(filename_);
	}
}

void FileInfo::clear()
{
	clearCustomData();

	etag_ = service_.make_etag(*this);
	mtime_.clear(); // gets computed on-demand
	mimetype_ = service_.get_mimetype(filename_);
}

} // namespace x0
