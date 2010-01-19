#include <x0/file.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

file::file(fileinfo_ptr fi, int flags) :
	fileinfo_(fi),
	fd_(::open(fileinfo_->filename().c_str(), flags))
{
	if (fd_ != -1)
	{
		fcntl(fd_, F_SETFL, O_CLOEXEC, 1);
	}
}

file::~file()
{
	if (fd_ != -1)
	{
		::close(fd_);
	}
}

} // namespace x0
