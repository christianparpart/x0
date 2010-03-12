#ifndef sw_x0_fileinfo_service_hpp
#define sw_x0_fileinfo_service_hpp (1)

#include <x0/cache.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>
#include <string>

#include <ev.h>

#if 0
#	define FILEINFO_DEBUG(msg...) printf("fileinfo_service: " msg)
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
class X0_API fileinfo_service :
	public boost::noncopyable
{
private:
	struct ::ev_loop *loop_;

	std::map<std::string, fileinfo_ptr> cache_;		//!< cache, storing path->fileinfo pairs

	bool etag_consider_mtime_;						//!< flag, specifying wether or not the file modification-time is part of the ETag
	bool etag_consider_size_;						//!< flag, specifying wether or not the file size is part of the ETag
	bool etag_consider_inode_;						//!< flag, specifying wether or not the file inode number is part of the ETag

	std::map<std::string, std::string> mimetypes_;	//!< cached database for file extension to mimetype mapping
	std::string default_mimetype_;					//!< default mimetype for those files we could not determine the mimetype.

public:
	explicit fileinfo_service(struct ::ev_loop *loop);
	~fileinfo_service();

	fileinfo_ptr query(const std::string& filename);
	fileinfo_ptr operator()(const std::string& filename);

	std::size_t size() const;
	bool empty() const;

	bool etag_consider_mtime() const;
	void etag_consider_mtime(bool value);

	bool etag_consider_size() const;
	void etag_consider_size(bool value);

	bool etag_consider_inode() const;
	void etag_consider_inode(bool value);

	void load_mimetypes(const std::string& filename);

	std::string default_mimetype() const;
	void default_mimetype(const std::string& value);

private:
	friend class fileinfo;

	std::string get_mimetype(const std::string& ext) const;
	std::string make_etag(const fileinfo& fi) const;
};

} // namespace x0

//@}

// {{{ implementation
#include <x0/io/fileinfo.hpp>

namespace x0 {

inline fileinfo_service::fileinfo_service(struct ::ev_loop *loop) :
	loop_(loop),
	cache_(),
	etag_consider_mtime_(true),
	etag_consider_size_(true),
	etag_consider_inode_(false),
	mimetypes_(),
	default_mimetype_("text/plain")
{
}

inline fileinfo_service::~fileinfo_service()
{
}

inline fileinfo_ptr fileinfo_service::query(const std::string& _filename)
{
	std::string filename(_filename[_filename.size() - 1] == '/' ? _filename.substr(0, _filename.size() - 1) : _filename);

	auto i = cache_.find(filename);
	if (i != cache_.end())
	{
		FILEINFO_DEBUG("query.cached(%s)\n", filename.c_str());
		return i->second;
	}

	if (fileinfo_ptr fi = fileinfo_ptr(new fileinfo(*this, filename)))
	{
		FILEINFO_DEBUG("query(%s).new\n", filename.c_str());
		fi->mimetype_ = get_mimetype(filename);
		fi->etag_ = make_etag(*fi);

		cache_[filename] = fi;

		return fi;
	}

	FILEINFO_DEBUG("query(%s) failed (%s)\n", filename.c_str(), strerror(errno));
	// either ::stat() or caching failed.

	return fileinfo_ptr();
}

inline fileinfo_ptr fileinfo_service::operator()(const std::string& filename)
{
	return query(filename);
}

inline std::size_t fileinfo_service::size() const
{
	return cache_.size();
}

inline bool fileinfo_service::empty() const
{
	return cache_.empty();
}

inline bool fileinfo_service::etag_consider_mtime() const
{
	return etag_consider_mtime_;
}

inline void fileinfo_service::etag_consider_mtime(bool value)
{
	etag_consider_mtime_ = value;
}

inline bool fileinfo_service::etag_consider_size() const
{
	return etag_consider_size_;
}

inline void fileinfo_service::etag_consider_size(bool value)
{
	etag_consider_size_ = value;
}

inline bool fileinfo_service::etag_consider_inode() const
{
	return etag_consider_inode_;
}

inline void fileinfo_service::etag_consider_inode(bool value)
{
	etag_consider_inode_ = value;
}

inline std::string fileinfo_service::default_mimetype() const
{
	return default_mimetype_;
}

inline void fileinfo_service::default_mimetype(const std::string& value)
{
	default_mimetype_ = value;
}

inline std::string fileinfo_service::get_mimetype(const std::string& filename) const
{
	std::size_t ndot = filename.find_last_of(".");
	std::size_t nslash = filename.find_last_of("/");

	if (ndot != std::string::npos && ndot > nslash)
	{
		std::string ext(filename.substr(ndot + 1));

		while (ext.size())
		{
			auto i = mimetypes_.find(ext);

			if (i != mimetypes_.end())
				return i->second;

			if (ext[ext.size() - 1] != '~')
				break;

			ext.resize(ext.size() - 1);
		}
	}

	return default_mimetype_;
}

inline std::string fileinfo_service::make_etag(const fileinfo& fi) const
{
	int count = 0;
	std::stringstream sstr;

	sstr << '"';

	if (etag_consider_mtime_)
	{
		if (count++) sstr << '-';
		sstr << fi->st_mtime;
	}

	if (etag_consider_size_)
	{
		if (count++) sstr << '-';
		sstr << fi->st_size;
	}

	if (etag_consider_inode_)
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
