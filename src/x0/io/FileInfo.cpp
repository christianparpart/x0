#include "FileInfo.h"
#include "FileInfoService.h"

namespace x0 {

FileInfo::FileInfo(FileInfoService& service, const std::string& filename) :
	service_(service),
	stat_(),
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
	custom_data.clear();

	etag_ = service_.make_etag(*this);
	mtime_.clear(); // gets computed on-demand
	mimetype_ = service_.get_mimetype(filename_);
}

} // namespace x0
