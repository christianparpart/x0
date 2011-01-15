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

namespace x0 {

#if defined(HAVE_SYS_INOTIFY_H)
#	undef HAVE_SYS_INOTIFY_H
#endif

FileInfoService::FileInfoService(struct ::ev_loop *loop, const Config *config) :
	loop_(loop),
#if defined(HAVE_SYS_INOTIFY_H)
	handle_(-1),
	inotify_(loop_),
	wd_(),
#endif
	config_(config),
	cache_()
{
#if defined(HAVE_SYS_INOTIFY_H)
	handle_ = inotify_init();
	if (handle_ != -1)
	{
		if (fcntl(handle_, F_SETFL, fcntl(handle_, F_GETFL) | O_NONBLOCK) < 0)
			fprintf(stderr, "Error setting nonblock/cloexec flags on inotify handle\n");

		if (fcntl(handle_, F_SETFD, fcntl(handle_, F_GETFD) | FD_CLOEXEC) < 0)
			fprintf(stderr, "Error setting cloexec flags on inotify handle\n");

		inotify_.set<FileInfoService, &FileInfoService::on_inotify>(this);
		inotify_.start(handle_, ev::READ);
	}
#endif
}

FileInfoService::~FileInfoService()
{
#if defined(HAVE_SYS_INOTIFY_H)
	if (handle_ != -1)
	{
		::close(handle_);
	}
#endif
}

FileInfoPtr FileInfoService::query(const std::string& _filename)
{
	std::string filename(_filename[_filename.size() - 1] == '/' ? _filename.substr(0, _filename.size() - 1) : _filename);

	auto i = cache_.find(filename);
	if (i != cache_.end())
	{
		FILEINFO_DEBUG("query.cached(%s)\n", filename.c_str());
		return i->second;
	}

	if (FileInfoPtr fi = FileInfoPtr(new FileInfo(*this, filename)))
	{
		fi->mimetype_ = get_mimetype(filename);
		fi->etag_ = make_etag(*fi);

#if defined(HAVE_SYS_INOTIFY_H)
		FILEINFO_DEBUG("query(%s).new\n", filename.c_str());
		int rv = handle_ != -1 && ::inotify_add_watch(handle_, filename.c_str(),
				IN_ONESHOT | IN_ATTRIB | IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT |
				IN_DELETE | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE);

		if (rv != -1)
		{
			cache_[filename] = fi;
			wd_[rv] = filename;
		}
#else
		FILEINFO_DEBUG("query(%s)!\n", filename.c_str());
#endif

		return fi;
	}

	FILEINFO_DEBUG("query(%s) failed (%s)\n", filename.c_str(), strerror(errno));
	// either ::stat() or caching failed.

	return FileInfoPtr();
}


void FileInfoService::on_inotify(ev::io& w, int revents)
{
	DEBUG("FileInfoService::on_inotify()");

	char buf[4096];
	ssize_t rv = ::read(handle_, &buf, sizeof(buf));

	if (rv > 0)
	{
		inotify_event *i = (inotify_event *)buf;
		inotify_event *e = i + rv;

		while (i < e && i->wd != 0)
		{
			auto wi = wd_.find(i->wd);
			if (wi != wd_.end())
			{
				auto k = cache_.find(wi->second);
				fprintf(stderr, "FileInfoService.on_inotify: invalidate: %s\n", k->first.c_str());
				// on_invalidate(k->first, k->second);
				cache_.erase(k);
				wd_.erase(wi);
			}
			i += sizeof(*i) + i->len;
		}
	}
}

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
