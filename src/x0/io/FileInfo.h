#ifndef sw_x0_fileinfo_hpp
#define sw_x0_fileinfo_hpp (1)

#include <x0/Api.h>
#include <x0/Types.h>
#include <string>
#include <map>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include <ev++.h>

namespace x0 {

class FileInfoService;
class plugin;

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

	std::string filename_;

	bool exists_;
	mutable std::string etag_;
	mutable std::string mtime_;
	mutable std::string mimetype_;

	//std::map<const plugin *, void *> data_;

	friend class FileInfoService;

public:
	FileInfo(FileInfoService& service, const std::string& filename);

	std::string filename() const;

	std::size_t size() const;
	time_t mtime() const;

	bool exists() const;
	bool is_directory() const;
	bool is_regular() const;
	bool is_executable() const;

	const ev_statdata * operator->() const;

	// custom data (gets cleared on file object modification)
	std::map<plugin *, CustomDataPtr> custom_data;

	// HTTP related high-level properties
	std::string etag() const;
	std::string last_modified() const;
	std::string mimetype() const;

	void clear();

	int open(int flags = O_RDONLY | O_NOATIME);

private:
	std::string get_mime_type(std::string ext) const;
};

//@}

} // namespace x0

#include <x0/io/FileInfo.cc>

#endif
