#include <x0/io/file_source.hpp>
#include <x0/io/source_visitor.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

/** initializes a file source.
 *
 * \param f the file to stream
 */
file_source::file_source(const file_ptr& f) :
	fd_source(f->handle(), 0, f->info().size()),
	file_(f)
{
}

file_source::file_source(const file_ptr& f, std::size_t offset, std::size_t size) :
	fd_source(f->handle(), offset, size),
	file_(f)
{
}

file_source::~file_source()
{
}

void file_source::accept(source_visitor& v)
{
	v.visit(*this);
}

} // namespace x0
