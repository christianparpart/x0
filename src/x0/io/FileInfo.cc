#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>
#include <ctime>

namespace x0 {

inline std::string FileInfo::filename() const
{
	return filename_;
}

inline bool FileInfo::exists() const
{
	return exists_;
}

inline std::size_t FileInfo::size() const
{
	return stat_.st_size;
}

inline time_t FileInfo::mtime() const
{
	return stat_.st_mtime;
}

inline bool FileInfo::is_directory() const
{
	return S_ISDIR(stat_.st_mode);
}

inline bool FileInfo::is_regular() const
{
	return S_ISREG(stat_.st_mode);
}

inline bool FileInfo::is_executable() const
{
	return stat_.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH);
}

inline const ev_statdata *FileInfo::operator->() const
{
	return &stat_;
}

inline std::string FileInfo::etag() const
{
	return etag_;
}

inline std::string FileInfo::last_modified() const
{
	if (mtime_.empty())
	{
		if (struct tm *tm = std::gmtime(&stat_.st_mtime))
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

// vim:syntax=cpp
