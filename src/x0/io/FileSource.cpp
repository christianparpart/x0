#include <x0/io/FileSource.h>
#include <x0/io/SourceVisitor.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

/** initializes a file source.
 *
 * \param f the file to stream
 */
FileSource::FileSource(const FilePtr& f) :
	SystemSource(f->handle(), 0, f->info().size()),
	file_(f)
{
}

FileSource::FileSource(const FilePtr& f, std::size_t offset, std::size_t size) :
	SystemSource(f->handle(), offset, size),
	file_(f)
{
}

FileSource::~FileSource()
{
}

void FileSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

} // namespace x0
