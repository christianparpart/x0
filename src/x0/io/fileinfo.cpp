#include "fileinfo.hpp"
#include "fileinfo_service.hpp"

namespace x0 {

fileinfo::fileinfo(fileinfo_service& service, const std::string& filename) :
	service_(service),
	watcher_(service.loop_),
	filename_(filename),
	exists_(false),
	etag_(),
	mtime_(),
	mimetype_()
{
	if (filename_.empty())
		return;

	watcher_.set<fileinfo, &fileinfo::callback>(this);
	watcher_.set(filename_.c_str());
	watcher_.start();

	if ((exists_ = watcher_.attr.st_nlink > 0))
		etag_ = service_.make_etag(*this);

	mimetype_ = service_.get_mimetype(filename_);

	DEBUG("fileinfo('%s') exists=%d, nlink=%d, size=%lld",
		filename_.c_str(), exists_, watcher_.attr.st_nlink, watcher_.attr.st_size);
}


void fileinfo::callback(ev::stat& w, int revents)
{
	custom_data.clear();

	etag_ = service_.make_etag(*this);
	mtime_.clear(); // gets computed on-demand
	mimetype_ = service_.get_mimetype(filename_);
}

} // namespace x0
