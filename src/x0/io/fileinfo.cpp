#include "fileinfo.hpp"
#include "fileinfo_service.hpp"

namespace x0 {

fileinfo::fileinfo(fileinfo_service& service, const std::string& filename) :
	service_(service),
	watcher_(service.loop_),
	filename_(filename),
	exists_(),
	etag_(service_.make_etag(*this)),
	mtime_(),
	mimetype_(service_.get_mimetype(filename_))
{
	watcher_.set<fileinfo, &fileinfo::callback>(this);
	watcher_.set(filename_.c_str());
	watcher_.start();

	exists_ = watcher_.attr.st_nlink > 0;
}


void fileinfo::callback(ev::stat& w, int revents)
{
	custom_data.clear();

	etag_ = service_.make_etag(*this);
	mtime_.clear(); // gets computed on-demand
	mimetype_ = service_.get_mimetype(filename_);
}

} // namespace x0
