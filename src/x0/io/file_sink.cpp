#include <x0/io/file_sink.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

file_sink::file_sink(const std::string& filename) :
	fd_sink(open(filename.c_str(), O_WRONLY | O_CREAT, 0666))
{
	fcntl(handle_, O_NONBLOCK, 1);
	fcntl(handle_, O_CLOEXEC, 1);
}

file_sink::~file_sink()
{
	::close(handle_);
}

} // namespace x0
