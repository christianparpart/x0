/* <FileInfoService.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_FileInfoService_h
#define sw_x0_FileInfoService_h (1)

#include <x0/Cache.h>
#include <x0/Types.h>
#include <x0/Api.h>
#include <x0/sysconfig.h>
#include <string>

#include <ev++.h>

#if defined(HAVE_SYS_INOTIFY_H)
#	include <sys/inotify.h>
#	include <sys/fcntl.h>
#endif

#if 0
#	define FILEINFO_DEBUG(msg...) printf("FileInfoService: " msg)
#else
#	define FILEINFO_DEBUG(msg...) /*!*/
#endif

namespace x0 {

//! \addtogroup core
//@{

/** service for retrieving file information.
 *
 * This is like stat(), in fact, it's using stat() and more magic, but
 * caches the result for further use and also invalidates in realtime the file-info items
 * in case their underlying inode has been updated.
 *
 * \note this class is not thread-safe
 */
class X0_API FileInfoService
{
public:
	struct Config
	{
		bool etagConsiderMtime;							//!< flag, specifying wether or not the file modification-time is part of the ETag
		bool etagConsiderSize;							//!< flag, specifying wether or not the file size is part of the ETag
		bool etagConsiderInode;							//!< flag, specifying wether or not the file inode number is part of the ETag

		std::map<std::string, std::string> mimetypes;	//!< cached database for file extension to mimetype mapping
		std::string defaultMimetype;					//!< default mimetype for those files we could not determine the mimetype.

		Config() :
			etagConsiderMtime(true),
			etagConsiderSize(true),
			etagConsiderInode(false),
			mimetypes(),
			defaultMimetype("text/plain")
		{}

		void loadMimetypes(const std::string& filename);
	};

private:
	struct ::ev_loop *loop_;
#if defined(HAVE_SYS_INOTIFY_H)
	int handle_;									//!< inotify handle
	ev::io inotify_;
	std::map<int, std::string> wd_;
#endif

	const Config *config_;
	std::map<std::string, FileInfoPtr> cache_;		//!< cache, storing path->FileInfo pairs

public:
	FileInfoService(struct ::ev_loop *loop, const Config *config);
	~FileInfoService();

	FileInfoService(const FileInfoService&) = delete;
	FileInfoService& operator=(const FileInfoService&) = delete;

	FileInfoPtr query(const std::string& filename);
	FileInfoPtr operator()(const std::string& filename);

	std::size_t size() const;
	bool empty() const;

private:
	friend class FileInfo;

	std::string get_mimetype(const std::string& ext) const;
	std::string make_etag(const FileInfo& fi) const;
	void on_inotify(ev::io& w, int revents);
};

} // namespace x0

//@}

// {{{ implementation
#include <x0/io/FileInfo.h>

namespace x0 {

inline FileInfoPtr FileInfoService::query(const std::string& _filename)
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
		FILEINFO_DEBUG("query(%s).new\n", filename.c_str());
		fi->mimetype_ = get_mimetype(filename);
		fi->etag_ = make_etag(*fi);

#if defined(HAVE_SYS_INOTIFY_H)
		int rv = handle_ != -1 && ::inotify_add_watch(handle_, filename.c_str(),
				IN_ONESHOT | IN_ATTRIB | IN_MODIFY | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT |
				IN_DELETE | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_MOVED_FROM | IN_MOVED_TO | IN_CREATE);;

		if (rv != -1)
		{
			cache_[filename] = fi;
			wd_[rv] = filename;
		}
#endif

		return fi;
	}

	FILEINFO_DEBUG("query(%s) failed (%s)\n", filename.c_str(), strerror(errno));
	// either ::stat() or caching failed.

	return FileInfoPtr();
}

inline FileInfoPtr FileInfoService::operator()(const std::string& filename)
{
	return query(filename);
}

inline std::size_t FileInfoService::size() const
{
	return cache_.size();
}

inline bool FileInfoService::empty() const
{
	return cache_.empty();
}

inline std::string FileInfoService::make_etag(const FileInfo& fi) const
{
	int count = 0;
	std::stringstream sstr;

	sstr << '"';

	// TODO encode numbers in hex than dec (should be a tick faster)

	if (config_->etagConsiderMtime)
	{
		if (count++) sstr << '-';
		sstr << fi->st_mtime;
	}

	if (config_->etagConsiderSize)
	{
		if (count++) sstr << '-';
		sstr << fi->st_size;
	}

	if (config_->etagConsiderInode)
	{
		if (count++) sstr << '-';
		sstr << fi->st_ino;
	}

	/// \todo support checksum etags (crc, md5, sha1, ...) - although, btrfs supports checksums directly on filesystem level!

	sstr << '"';

	return sstr.str();
}

} // namespace x0

// }}}

#endif
