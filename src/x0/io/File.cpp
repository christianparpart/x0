#include <x0/io/File.h>
#include <x0/io/FileInfo.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

File::File(FileInfoPtr fi, int flags) :
	fileinfo_(fi),
	fd_(::open(fileinfo_->filename().c_str(), flags))
{
	if (fd_ != -1)
	{
		fcntl(fd_, F_SETFL, O_CLOEXEC, 1);
	}
}

File::~File()
{
	if (fd_ != -1)
	{
		::close(fd_);
		fd_ = -1;
	}
}

} // namespace x0
