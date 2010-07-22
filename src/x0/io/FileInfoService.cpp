/* <x0/FileInfoService.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FileInfoService.h>
#include <x0/strutils.h>

#include <boost/tokenizer.hpp>

namespace x0 {

FileInfoService::FileInfoService(struct ::ev_loop *loop) :
	loop_(loop),
#if defined(HAVE_SYS_INOTIFY_H)
	handle_(),
	inotify_(loop_),
	wd_(),
#endif
	cache_(),
	etag_consider_mtime_(true),
	etag_consider_size_(true),
	etag_consider_inode_(false),
	mimetypes_(),
	default_mimetype_("text/plain")
{
#if defined(HAVE_SYS_INOTIFY_H)
	handle_ = inotify_init();
	if (handle_ != -1)
	{
		fcntl(handle_, F_SETFL, O_NONBLOCK | FD_CLOEXEC);

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

void FileInfoService::on_inotify(ev::io& w, int revents)
{
	//DEBUG("FileInfoService::on_inotify()");

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
				// on_invalidate(k->first, k->second);
				cache_.erase(k);
				wd_.erase(wi);
			}
			i += sizeof(*i) + i->len;
		}
	}
}

void FileInfoService::load_mimetypes(const std::string& filename)
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

	std::string input(x0::read_file(filename));
	tokenizer lines(input, boost::char_separator<char>("\n"));

	mimetypes_.clear();

	for (tokenizer::iterator i = lines.begin(), e = lines.end(); i != e; ++i)
	{
		std::string line(x0::trim(*i));
		tokenizer columns(line, boost::char_separator<char>(" \t"));

		tokenizer::iterator ci = columns.begin(), ce = columns.end();
		std::string mime = ci != ce ? *ci++ : std::string();

		if (!mime.empty() && mime[0] != '#')
		{
			for (; ci != ce; ++ci)
			{
				mimetypes_[*ci] = mime;
			}
		}
	}
}

} // namespace x0
