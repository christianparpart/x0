/* <FileInfo.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_fileinfo_hpp
#define sw_x0_fileinfo_hpp (1)

#include <x0/Api.h>
#include <x0/Types.h>
#include <string>
#include <unordered_map>

#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <ev++.h>

namespace x0 {

class FileInfoService;
class HttpPlugin;

//! \addtogroup core
//@{

/** file info cache object.
 *
 * \see FileInfoService, server
 */
class X0_API FileInfo
{
private:
	FileInfo(const FileInfo&) = delete;
	FileInfo& operator=(const FileInfo&) = delete;

private:
	FileInfoService& service_;

	struct stat stat_;

	int inotifyId_;
	ev_tstamp cachedAt_;

	std::string filename_;

	bool exists_;
	mutable std::string etag_;
	mutable std::string mtime_;
	mutable std::string mimetype_;

	friend class FileInfoService;

public:
	FileInfo(FileInfoService& service, const std::string& filename);

	const std::string& filename() const { return filename_; }

	ev_tstamp cachedAt() const { return cachedAt_; }

	std::size_t size() const { return stat_.st_size; }
	time_t mtime() const { return stat_.st_mtime; }

	bool exists() const { return exists_; }
	bool isDirectory() const { return S_ISDIR(stat_.st_mode); }
	bool isRegular() const { return S_ISREG(stat_.st_mode); }
	bool isExecutable() const { return stat_.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH); }

	const ev_statdata * operator->() const { return &stat_; }

	// custom data (gets cleared on file object modification)
	std::unordered_map<HttpPlugin *, CustomDataPtr> customData;

	// HTTP related high-level properties
	std::string etag() const;
	std::string lastModified() const;
	std::string mimetype() const;

	void clear();

	int open(int flags = O_RDONLY | O_NOATIME);

private:
	std::string get_mime_type(std::string ext) const;
};

//@}

} // namespace x0

// {{{ inlines
namespace x0 {

inline std::string FileInfo::etag() const
{
	return etag_;
}

inline std::string FileInfo::lastModified() const
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

inline std::string FileInfo::mimetype() const
{
	return mimetype_;
}

inline int FileInfo::open(int flags)
{
#if defined(O_LARGEFILE)
	flags |= O_LARGEFILE;
#endif

	return ::open(filename_.c_str(), flags);
}

} // namespace x0
// }}}

#endif
