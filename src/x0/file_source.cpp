#include <x0/file_source.hpp>
#include <x0/source_visitor.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

file_source::file_source(const std::string& filename) :
	fd_source(open(filename.c_str(), O_RDONLY))
{
	fcntl(handle_, F_SETFL, O_CLOEXEC, 1);
}

file_source::~file_source()
{
	::close(handle_);
}

void file_source::accept(source_visitor& v)
{
	v.visit(*this);
}

} // namespace x0
