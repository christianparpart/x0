#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <ctime>

namespace x0 {

inline fileinfo::fileinfo(const std::string& filename) :
	filename_(filename),
	exists_(),
	etag_(),
	mtime_(),
	mimetype_()
{
	if (::stat(filename_.c_str(), &st_) == 0)
	{
		exists_ = true;
	}
	else
	{
		//DEBUG("fileinfo: could not stat file: %s: %s", filename_.c_str(), strerror(errno));
		exists_ = false;
		std::memset(&st_, 0, sizeof(st_));
	}
}

inline std::string fileinfo::filename() const
{
	return filename_;
}

inline bool fileinfo::exists() const
{
	return exists_;
}

inline std::size_t fileinfo::size() const
{
	return st_.st_size;
}

inline time_t fileinfo::mtime() const
{
	return st_.st_mtime;
}

inline bool fileinfo::is_directory() const
{
	return S_ISDIR(st_.st_mode);
}

inline bool fileinfo::is_regular() const
{
	return S_ISREG(st_.st_mode);
}

inline bool fileinfo::is_executable() const
{
	return st_.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
}

inline std::string fileinfo::etag() const
{
	return etag_;
}

inline std::string fileinfo::last_modified() const
{
	if (mtime_.empty())
	{
		if (struct tm *tm = std::gmtime(&st_.st_mtime))
		{
			char buf[256];

			if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm) != 0)
			{
				mtime_ = buf;
			}
		}
	}

	return mtime_;
}

inline std::string fileinfo::mimetype() const
{
	return mimetype_;
}

} // namespace x0

// vim:syntax=cpp
