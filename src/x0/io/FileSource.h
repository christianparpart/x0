#ifndef sw_x0_io_FileSource_hpp
#define sw_x0_io_FileSource_hpp 1

#include <x0/io/SystemSource.h>
#include <x0/io/File.h>
#include <string>

namespace x0 {

//! \addtogroup io
//@{

/** file source.
 */
class X0_API FileSource :
	public SystemSource
{
private:
	FilePtr file_;

public:
	explicit FileSource(const FilePtr& f);
	FileSource(const FilePtr& f, std::size_t offset, std::size_t count);
	~FileSource();

	FilePtr file() const;

	virtual void accept(SourceVisitor& v);
};

//@}

inline FilePtr FileSource::file() const
{
	return file_;
}

} // namespace x0

#endif
