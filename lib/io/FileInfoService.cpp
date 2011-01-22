/* <x0/FileInfoService.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FileInfoService.h>
#include <x0/strutils.h>
#include <x0/sysconfig.h>

#include <boost/tokenizer.hpp>

#if defined(HAVE_SYS_INOTIFY_H)
#undef HAVE_SYS_INOTIFY_H
#endif

namespace x0 {

FileInfoService::FileInfoService(struct ::ev_loop *loop, const Config *config) :
	loop_(loop),
#if defined(HAVE_SYS_INOTIFY_H)
	handle_(-1),
	inotify_(loop_),
	inotifies_(),
#endif
	config_(config),
	cache_()
{
#if defined(HAVE_SYS_INOTIFY_H)
	handle_ = inotify_init();
	if (handle_ != -1) {
		if (fcntl(handle_, F_SETFL, fcntl(handle_, F_GETFL) | O_NONBLOCK) < 0)
			fprintf(stderr, "Error setting nonblock/cloexec flags on inotify handle\n");

		if (fcntl(handle_, F_SETFD, fcntl(handle_, F_GETFD) | FD_CLOEXEC) < 0)
			fprintf(stderr, "Error setting cloexec flags on inotify handle\n");

		inotify_.set<FileInfoService, &FileInfoService::onFileChanged>(this);
		inotify_.start(handle_, ev::READ);
	} else {
		fprintf(stderr, "Error initializing inotify: %s\n", strerror(errno));
	}
#endif
}

FileInfoService::~FileInfoService()
{
#if defined(HAVE_SYS_INOTIFY_H)
	if (handle_ != -1) {
		::close(handle_);
	}
#endif
}

FileInfoPtr FileInfoService::query(const std::string& _filename)
{
	std::string filename(_filename[_filename.size() - 1] == '/'
			? _filename.substr(0, _filename.size() - 1)
			: _filename);

	auto i = cache_.find(filename);
	if (i != cache_.end()) {
		FileInfoPtr fi = i->second;
		if (fi->inotifyId_ > 0 || fi->cachedAt() + config_->cacheTTL_ > ev_now(loop_)) {
			FILEINFO_DEBUG("query.cached(%s)\n", filename.c_str());
			return fi;
		}

		FILEINFO_DEBUG("query.expired(%s)\n", filename.c_str());
#if defined(HAVE_SYS_INOTIFY_H)
		inotifies_.erase(inotifies_.find(fi->inotifyId_));
#endif
		cache_.erase(i);
	}

	if (FileInfoPtr fi = FileInfoPtr(new FileInfo(*this, filename))) {
		fi->mimetype_ = get_mimetype(filename);
		fi->etag_ = make_etag(*fi);

#if defined(HAVE_SYS_INOTIFY_H)
		int rv = handle_ != -1 && ::inotify_add_watch(handle_, filename.c_str(),
			/*IN_ONESHOT |*/ IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT |
			IN_DELETE_SELF | IN_MOVE_SELF
		);
		FILEINFO_DEBUG("query(%s).new -> %d\n", filename.c_str(), rv);

		if (rv != -1) {
			fi->inotifyId_ = rv;
			inotifies_[rv] = fi;
		}
		cache_[filename] = fi;
#else
		FILEINFO_DEBUG("query(%s)!\n", filename.c_str());
		cache_[filename] = fi;
#endif

		return fi;
	}

	FILEINFO_DEBUG("query(%s) failed (%s)\n", filename.c_str(), strerror(errno));
	// either ::stat() or caching failed.

	return FileInfoPtr();
}


#if defined(HAVE_SYS_INOTIFY_H)
void FileInfoService::onFileChanged(ev::io& w, int revents)
{
	FILEINFO_DEBUG("onFileChanged()\n");

	char buf[sizeof(inotify_event) * 256];
	ssize_t rv = ::read(handle_, &buf, sizeof(buf));
	FILEINFO_DEBUG("read returned: %ld (%% %ld, %ld)\n",
			rv, sizeof(inotify_event), rv / sizeof(inotify_event));

	if (rv > 0) {
		const char *i = buf;
		const char *e = i + rv;
		inotify_event *ev = (inotify_event *)i;

		for (; i < e && ev->wd != 0; i += sizeof(*ev) + ev->len, ev = (inotify_event *)i) {
			FILEINFO_DEBUG("traverse: (wd:%d, mask:0x%04x, cookie:%d)\n", ev->wd, ev->mask, ev->cookie);
			auto wi = inotifies_.find(ev->wd);
			if (wi == inotifies_.end()) {
				FILEINFO_DEBUG("-skipping\n");
				continue;
			}

			auto k = cache_.find(wi->second->filename());
			FILEINFO_DEBUG("invalidate: %s\n", k->first.c_str());
			// onInvalidate(k->first, k->second);
			cache_.erase(k);
			inotifies_.erase(wi);
			int rv = inotify_rm_watch(handle_, ev->wd);
			if (rv < 0) {
				FILEINFO_DEBUG("error removing inotify watch (%d, %s): %s\n", ev->wd, ev->name, strerror(errno));
			} else {
				FILEINFO_DEBUG("inotify_rm_watch: %d (ok)\n", rv);
			}
		}
	}
}
#endif

void FileInfoService::Config::loadMimetypes(const std::string& filename)
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

	std::string input(x0::read_file(filename));
	tokenizer lines(input, boost::char_separator<char>("\n"));

	mimetypes.clear();

	for (auto line: lines)
	{
		line = x0::trim(line);
		tokenizer columns(line, boost::char_separator<char>(" \t"));

		tokenizer::iterator ci = columns.begin(), ce = columns.end();
		std::string mime = ci != ce ? *ci++ : std::string();

		if (!mime.empty() && mime[0] != '#')
		{
			for (; ci != ce; ++ci)
			{
				mimetypes[*ci] = mime;
			}
		}
	}
}

std::string FileInfoService::get_mimetype(const std::string& filename) const
{
	std::size_t ndot = filename.find_last_of(".");
	std::size_t nslash = filename.find_last_of("/");

	if (ndot != std::string::npos && ndot > nslash)
	{
		std::string ext(filename.substr(ndot + 1));

		while (ext.size())
		{
			auto i = config_->mimetypes.find(ext);

			if (i != config_->mimetypes.end())
			{
				//DEBUG("filename(%s), ext(%s), use mimetype: %s", filename.c_str(), ext.c_str(), i->second.c_str());
				return i->second;
			}

			if (ext[ext.size() - 1] != '~')
				break;

			ext.resize(ext.size() - 1);
		}
	}

	//DEBUG("file(%s) use default mimetype", filename.c_str());
	return config_->defaultMimetype;
}

} // namespace x0
