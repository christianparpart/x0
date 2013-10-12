#include <x0/http/HttpFile.h>
#include <x0/http/HttpFileMgr.h>
#include <x0/Buffer.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

namespace x0 {

HttpFile::HttpFile(const std::string& path, HttpFileMgr* mgr) :
	path_(path),
	fd_(-1),
	stat_(),
	refs_(0),
	etag_(),
	mtime_(),
	mimetype_()
{
	open(mgr);
}

HttpFile::~HttpFile()
{
	if (fd_ >= 0) {
		::close(fd_);
	}
}

bool HttpFile::open(HttpFileMgr* mgr)
{
	int flags = O_RDONLY;

#if 0 // defined(O_NOATIME)
	flags |= O_NOATIME;
#endif

#if defined(O_CLOEXEC)
	flags |= O_CLOEXEC;
#endif

	fd_ = ::open(path_.c_str(), flags);
	if (fd_ < 0)
		return false;

	return update(mgr);

	return true;
}

bool HttpFile::update(HttpFileMgr* mgr)
{
	int rv = fstat(fd_, &stat_);
	if (rv < 0)
		return false;

	if (mgr == nullptr) {
		mimetype_.clear();
		return true;
	}

	// compute entity tag
	{
		size_t count = 0;
		Buffer buf(256);
		buf.push_back('"');

		if (mgr->settings_->etagConsiderMtime) {
			if (count++) buf.push_back('-');
			buf.push_back(stat_.st_mtime);
		}

		if (mgr->settings_->etagConsiderSize) {
			if (count++) buf.push_back('-');
			buf.push_back(stat_.st_size);
		}

		if (mgr->settings_->etagConsiderInode) {
			if (count++) buf.push_back('-');
			buf.push_back(stat_.st_ino);
		}

		buf.push_back('"');

		etag_ = buf.str();
	}

	// query mimetype
	std::size_t ndot = path_.find_last_of(".");
	std::size_t nslash = path_.find_last_of("/");

	if (ndot != std::string::npos && ndot > nslash) {
		const auto& mimetypes = mgr->settings_->mimetypes;
		std::string ext(path_.substr(ndot + 1));

		mimetype_.clear();

		while (ext.size()) {
			auto i = mimetypes.find(ext);

			if (i != mimetypes.end()) {
				//DEBUG("filename(%s), ext(%s), use mimetype: %s", path_.c_str(), ext.c_str(), i->second.c_str());
				mimetype_ = i->second;
			}

			if (ext[ext.size() - 1] != '~')
				break;

			ext.resize(ext.size() - 1);
		}

		if (mimetype_.empty()) {
			mimetype_ = mgr->settings_->defaultMimetype;
		}
	} else {
		mimetype_ = mgr->settings_->defaultMimetype;
	}

	return true;
}

void HttpFile::close()
{
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = -1;
	}
}

void HttpFile::ref() {
	++refs_;
//	printf("HttpFile[%s].ref() -> %i\n", path_.c_str(), refs_);
}

void HttpFile::unref() {
	--refs_;
//	printf("HttpFile[%s].unref() -> %i\n", path_.c_str(), refs_);

	if (refs_ <= 0)
		delete this;
}

const std::string& HttpFile::lastModified() const
{
	if (mtime_.empty()) {
		if (struct tm *tm = std::gmtime(&stat_.st_mtime)) {
			char buf[256];

			if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm) != 0) {
				mtime_ = buf;
			}
		}
	}

	return mtime_;
}
} // namespace x0
